#include <Editor/Panels/AnimatorPanel.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cmath>
#include <cstdio>


namespace ed = ax::NodeEditor;

namespace Antelope::Editor
{
    static ed::NodeId EntryNodeId() { return 1; }
    static ed::PinId EntryOutPin() { return 2; }
    static ed::NodeId StateNode(uint32_t i) { return 10 + i * 3; }
    static ed::PinId StateInPin(uint32_t i) { return 10 + i * 3 + 1; }
    static ed::PinId StateOutPin(uint32_t i) { return 10 + i * 3 + 2; }
    static ed::LinkId TransitionLink(uint32_t j) { return 10000 + j; }

    AnimatorPanel::AnimatorPanel()
    {
        ed::Config cfg;
        cfg.SettingsFile = nullptr;
        m_NodeEditorCtx = ed::CreateEditor(&cfg);
    }

    AnimatorPanel::~AnimatorPanel()
    {
        if (m_NodeEditorCtx) { ed::DestroyEditor(m_NodeEditorCtx); }
    }

    void AnimatorPanel::SetModelCache(const std::unordered_map<uint64_t, ModelData>* cache)
    {
        m_ModelCache = cache;
    }

    void AnimatorPanel::OnUIRender(Entity selectedEntity)
    {
        ImGui::Begin("Animator");

        if (!selectedEntity || !selectedEntity.HasComponent<AnimatorComponent>())
        {
            ImGui::End();
            return;
        }

        auto& anim { selectedEntity.GetComponent<AnimatorComponent>() };
        uint64_t entityID { static_cast<uint64_t>(selectedEntity.GetHandle()) };

        if (entityID != m_LastEntityID)
        {
            m_LastEntityID = entityID;
            m_ShouldLayoutNodes = true;
            m_SelectedState = UINT32_MAX;
            m_SelectedLink = UINT32_MAX;
            m_PreviewTime = 0.0f;
            m_IsRenamingState = false;
        }

        ImGui::BeginChild("##AnimSidebar", ImVec2(180.0f, 0.0f), true);
        DrawParametersSidebar(anim);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##AnimRight", ImVec2(0.0f, 0.0f), false);

        {
            float inspectorH { 180.0f };
            float canvasH { ImGui::GetContentRegionAvail().y - inspectorH - ImGui::GetStyle().ItemSpacing.y };

            ImGui::BeginChild("##AnimCanvas", ImVec2(0.0f, canvasH), false);
            DrawNodeCanvas(anim);
            ImGui::EndChild();

            ImGui::BeginChild("##AnimInspector", ImVec2(0.0f, inspectorH), true);
            DrawInspectorPanel(anim);
            ImGui::EndChild();
        }

        ImGui::EndChild();

        ImGui::End();
    }

    void AnimatorPanel::DrawParametersSidebar(AnimatorComponent& anim)
    {
        ImGui::TextDisabled("Parameters");
        ImGui::Separator();

        uint32_t toDelete { UINT32_MAX };

        for (uint32_t i { 0 }; i < static_cast<uint32_t>(anim.Controller.Parameters.size()); ++i)
        {
            auto& p { anim.Controller.Parameters[i] };
            ImGui::PushID(static_cast<int>(i));

            const char* tag { "F" };

            switch (p.ParamType)
            {
                case AnimatorParameter::Type::Int: tag = "I"; break;
                case AnimatorParameter::Type::Bool: tag = "B"; break;
                case AnimatorParameter::Type::Trigger: tag = "T"; break;
                default: break;
            }

            ImGui::TextDisabled("[%s]", tag);
            ImGui::SameLine();
            float xW { ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x };
            char buf[64];
            std::strncpy(buf, p.Name.c_str(), 63);
            buf[63] = '\0';
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - xW);

            if (ImGui::InputText("##pn", buf, sizeof(buf)))
            {
                p.Name = buf;
            }
            
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) { toDelete = i; }
            
            ImGui::PopID();
        }

        if (toDelete != UINT32_MAX)
        {
            anim.Controller.Parameters.erase(anim.Controller.Parameters.begin() + toDelete);

            if (toDelete < anim.FloatValues.size()) anim.FloatValues.erase(anim.FloatValues.begin() + toDelete);
            if (toDelete < anim.IntValues.size()) anim.IntValues.erase(anim.IntValues.begin() + toDelete);
            if (toDelete < anim.BoolValues.size()) anim.BoolValues.erase(anim.BoolValues.begin() + toDelete);
            if (toDelete < anim.TriggerValues.size()) anim.TriggerValues.erase(anim.TriggerValues.begin() + toDelete);

            for (auto& tr : anim.Controller.Transitions)
            {
                tr.Conditions.erase(
                    std::remove_if(tr.Conditions.begin(), tr.Conditions.end(), [toDelete](const TransitionCondition& c){ return c.ParameterIndex == toDelete; }),
                    tr.Conditions.end());

                for (auto& c : tr.Conditions)
                {
                    if (c.ParameterIndex > toDelete) { c.ParameterIndex--; }
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##pname", m_NewParamName, sizeof(m_NewParamName));
        const char* types[] { "Float", "Int", "Bool", "Trigger" };
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##ptype", &m_NewParamType, types, 4);

        if (ImGui::Button("Add Parameter", ImVec2(-1.0f, 0.0f)) && m_NewParamName[0] != '\0')
        {
            AnimatorParameter p {};
            p.Name = m_NewParamName;
            p.ParamType = static_cast<AnimatorParameter::Type>(m_NewParamType);
            anim.Controller.Parameters.push_back(p);
            anim.FloatValues.push_back(0.0f);
            anim.IntValues.push_back(0);
            anim.BoolValues.push_back(false);
            anim.TriggerValues.push_back(false);
        }
    }

    void AnimatorPanel::DrawNodeCanvas(AnimatorComponent& anim)
    {
        ed::SetCurrentEditor(m_NodeEditorCtx);
        ed::Begin("##AnimCanvas", ImVec2(0.0f, 0.0f));

        if (m_ShouldLayoutNodes)
        {
            m_ShouldLayoutNodes = false;
            ed::SetNodePosition(EntryNodeId(), ImVec2(-180.0f, 30.0f));

            for (uint32_t i { 0 }; i < static_cast<uint32_t>(anim.Controller.States.size()); ++i)
            {
                auto& s { anim.Controller.States[i] };
                ImVec2 pos { (s.EditorPos.x != 0.0f || s.EditorPos.y != 0.0f)
                           ? ImVec2(s.EditorPos.x, s.EditorPos.y)
                           : ImVec2(50.0f + i * 230.0f, 30.0f) };
                ed::SetNodePosition(StateNode(i), pos);
            }

            m_LastStateCount = static_cast<uint32_t>(anim.Controller.States.size());
        }
        else if (anim.Controller.States.size() > m_LastStateCount)
        {
            for (uint32_t i { m_LastStateCount }; i < static_cast<uint32_t>(anim.Controller.States.size()); ++i)
            {
                ed::SetNodePosition(StateNode(i), ImVec2(50.0f + i * 230.0f, 30.0f));
            }

            m_LastStateCount = static_cast<uint32_t>(anim.Controller.States.size());
        }
        else
        {
            m_LastStateCount = static_cast<uint32_t>(anim.Controller.States.size());
        }

        ed::BeginNode(EntryNodeId());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        ImGui::TextUnformatted("  Entry  ");
        ImGui::PopStyleColor();
        ed::BeginPin(EntryOutPin(), ed::PinKind::Output);
        ImGui::TextUnformatted("  >");
        ed::EndPin();
        ed::EndNode();

        for (uint32_t i { 0 }; i < static_cast<uint32_t>(anim.Controller.States.size()); ++i)
        {
            auto& state { anim.Controller.States[i] };
            bool isDefault { i == anim.Controller.DefaultStateIndex };

            ed::BeginNode(StateNode(i));

            ed::BeginPin(StateInPin(i), ed::PinKind::Input);
            ImGui::TextUnformatted("< ");
            ed::EndPin();
            ImGui::SameLine();

            if (isDefault) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f)); }
            ImGui::Text("%-14s", state.Name.c_str());
            if (isDefault) { ImGui::PopStyleColor(); }
            ImGui::SameLine();

            ed::BeginPin(StateOutPin(i), ed::PinKind::Output);
            ImGui::TextUnformatted(" >");
            ed::EndPin();

            if (state.ClipIndex < anim.Controller.Clips.size())
            {
                const auto& clip { anim.Controller.Clips[state.ClipIndex] };
                float dur { clip.TicksPerSecond > 0.0f ? clip.Duration / clip.TicksPerSecond : 0.0f };
                ImGui::TextDisabled("  %s  %.1fs", clip.Name.c_str(), dur);
            }
            else
            {
                ImGui::TextDisabled("  [drop clip here]");
            }

            ed::EndNode();
        }

        for (uint32_t j { 0 }; j < static_cast<uint32_t>(anim.Controller.Transitions.size()); ++j)
        {
            const auto& t { anim.Controller.Transitions[j] };
            ImVec4 col { (m_SelectedLink == j) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.75f, 0.75f, 0.75f, 1.0f) };
            ed::PinId sp { t.FromState == AnimationTransition::ENTRY ? EntryOutPin() : StateOutPin(t.FromState) };
            ed::Link(TransitionLink(j), sp, StateInPin(t.ToState), col, 2.0f);
        }

        if (ed::BeginCreate())
        {
            ed::PinId sp, ep;

            if (ed::QueryNewLink(&sp, &ep))
            {
                constexpr uint32_t NOT_FOUND { UINT32_MAX - 1u };

                auto resolve = [&](ed::PinId pin, bool wantOut) -> uint32_t
                {
                    if (wantOut && pin == EntryOutPin()) { return AnimationTransition::ENTRY; }

                    for (uint32_t i { 0 }; i < anim.Controller.States.size(); ++i)
                    {
                        if ( wantOut && pin == StateOutPin(i)) { return i; }
                        if (!wantOut && pin == StateInPin(i)) { return i; }
                    }

                    return NOT_FOUND;
                };

                uint32_t from { resolve(sp, true) };
                uint32_t to { resolve(ep, false) };

                if (from == NOT_FOUND || to == NOT_FOUND)
                {
                    from = resolve(ep, true);
                    to = resolve(sp, false);
                }

                bool valid { from != NOT_FOUND && to != NOT_FOUND && to != AnimationTransition::ENTRY && from != to };

                if (valid)
                {
                    if (ed::AcceptNewItem(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), 2.0f))
                    {
                        AnimationTransition tr;
                        tr.FromState = from;
                        tr.ToState = to;
                        anim.Controller.Transitions.push_back(tr);
                    }
                }
                else { ed::RejectNewItem(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 2.0f); }
            }
        }
        ed::EndCreate();

        if (ed::BeginDelete())
        {
            ed::LinkId dl;

            while (ed::QueryDeletedLink(&dl))
            {
                if (ed::AcceptDeletedItem())
                {
                    uint32_t lid { static_cast<uint32_t>(dl.Get()) };

                    if (lid >= 10000)
                    {
                        uint32_t idx { lid - 10000 };

                        if (idx < anim.Controller.Transitions.size())
                        {
                            anim.Controller.Transitions.erase(anim.Controller.Transitions.begin() + idx);
                        }

                        if (m_SelectedLink == idx) { m_SelectedLink = UINT32_MAX; }
                    }
                }
            }

            ed::NodeId dn;

            while (ed::QueryDeletedNode(&dn))
            {
                if (ed::AcceptDeletedItem())
                {
                    uint32_t nid { static_cast<uint32_t>(dn.Get()) };

                    if (nid >= 10)
                    {
                        uint32_t si { (nid - 10) / 3 };

                        if (si < anim.Controller.States.size())
                        {
                            anim.Controller.Transitions.erase(
                                std::remove_if(anim.Controller.Transitions.begin(), anim.Controller.Transitions.end(),
                                               [si](const AnimationTransition& t){ return t.FromState == si || t.ToState == si; }),
                                anim.Controller.Transitions.end());

                            for (auto& t : anim.Controller.Transitions)
                            {
                                if (t.FromState != AnimationTransition::ENTRY && t.FromState > si) { t.FromState--; }
                                if (t.ToState > si) { t.ToState--; }
                            }

                            anim.Controller.States.erase(anim.Controller.States.begin() + si);

                            if (anim.Controller.DefaultStateIndex == si) { anim.Controller.DefaultStateIndex = 0; }
                            else if (anim.Controller.DefaultStateIndex > si) { anim.Controller.DefaultStateIndex--; }

                            if (m_SelectedState == si) { m_SelectedState = UINT32_MAX; }
                            else if (m_SelectedState > si && m_SelectedState != UINT32_MAX) { m_SelectedState--; }
                        }
                    }
                }
            }
        }
        ed::EndDelete();

        {
            ed::NodeId ctxNode;
            ed::LinkId ctxLink;
            bool showState { false }, showLink { false }, showBg { false };

            if (ed::ShowNodeContextMenu(&ctxNode))
            {
                uint32_t nid { static_cast<uint32_t>(ctxNode.Get()) };
                m_ContextMenuState = (nid >= 10) ? (nid - 10) / 3 : UINT32_MAX;
                m_IsRenamingState = false;
                showState = true;
            }
            else if (ed::ShowLinkContextMenu(&ctxLink))
            {
                uint32_t lid { static_cast<uint32_t>(ctxLink.Get()) };
                m_ContextLinkIdx = (lid >= 10000) ? lid - 10000 : UINT32_MAX;
                showLink = true;
            }
            else if (ed::ShowBackgroundContextMenu())
            {
                showBg = true;
            }

            ed::Suspend();

            if (showState) { ImGui::OpenPopup("##StateCtx"); }
            if (showLink) { ImGui::OpenPopup("##TransCtx"); }
            if (showBg) { ImGui::OpenPopup("##CanvasCtx"); }

            if (ImGui::BeginPopup("##StateCtx"))
            {
                if (m_ContextMenuState < anim.Controller.States.size())
                {
                    auto& cs { anim.Controller.States[m_ContextMenuState] };

                    if (m_IsRenamingState)
                    {
                        ImGui::SetNextItemWidth(160.0f);
                        bool ok { ImGui::InputText("##rn", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue) };

                        if (ok || ImGui::Button("OK", ImVec2(75.0f, 0.0f)))
                        {
                            if (m_RenameBuffer[0] != '\0') { cs.Name = m_RenameBuffer; }
                            m_IsRenamingState = false;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::SameLine();

                        if (ImGui::Button("Cancel", ImVec2(75.0f, 0.0f)))
                        {
                            m_IsRenamingState = false;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("%s", cs.Name.c_str());
                        ImGui::Separator();

                        if (ImGui::MenuItem("Set as Default"))
                        {
                            anim.Controller.DefaultStateIndex = m_ContextMenuState;
                        }

                        if (ImGui::Button("Rename", ImVec2(-1.0f, 0.0f)))
                        {
                            std::strncpy(m_RenameBuffer, cs.Name.c_str(), 63);
                            m_RenameBuffer[63] = '\0';
                            m_IsRenamingState = true;
                        }

                        if (ImGui::BeginMenu("Assign Clip"))
                        {
                            if (anim.Controller.Clips.empty())
                            {
                                ImGui::TextDisabled("Drag a clip in the Inspector");
                            }
                            else
                            {
                                for (uint32_t c { 0 }; c < static_cast<uint32_t>(anim.Controller.Clips.size()); ++c)
                                {
                                    bool cur { cs.ClipIndex == c };

                                    if (ImGui::MenuItem(anim.Controller.Clips[c].Name.c_str(), nullptr, cur))
                                    {
                                        cs.ClipIndex = c;
                                    }
                                }
                            }
                            ImGui::EndMenu();
                        }

                        ImGui::Separator();

                        if (ImGui::MenuItem("Delete")) { ed::DeleteNode(StateNode(m_ContextMenuState)); }
                    }
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("##TransCtx"))
            {
                if (m_ContextLinkIdx < anim.Controller.Transitions.size())
                {
                    auto& tr { anim.Controller.Transitions[m_ContextLinkIdx] };
                    ImGui::TextDisabled("Transition");
                    ImGui::Separator();
                    ImGui::DragFloat("Blend", &tr.BlendDuration, 0.01f, 0.0f, 2.0f, "%.2fs");
                    ImGui::Checkbox("Exit Time", &tr.HasExitTime);

                    if (tr.HasExitTime) { ImGui::DragFloat("Exit At", &tr.ExitTime, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp); }
                    
                    ImGui::Separator();

                    if (ImGui::MenuItem("Delete")) { ed::DeleteLink(TransitionLink(m_ContextLinkIdx)); }
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("##CanvasCtx"))
            {
                ImGui::TextDisabled("New State");
                ImGui::Separator();
                ImGui::SetNextItemWidth(160.0f);
                ImGui::InputText("##ns", m_NewStateName, sizeof(m_NewStateName));

                if (ImGui::Button("Add##ns", ImVec2(160.0f, 0.0f)) && m_NewStateName[0] != '\0')
                {
                    AnimationStateNode ns;
                    ns.Name = m_NewStateName;
                    ns.ClipIndex = UINT32_MAX;
                    ns.Speed = 1.0f;
                    ns.Loop = true;
                    anim.Controller.States.push_back(ns);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ed::Resume();
        }

        ed::End();

        for (uint32_t i { 0 }; i < static_cast<uint32_t>(anim.Controller.States.size()); ++i)
        {
            ImVec2 pos { ed::GetNodePosition(StateNode(i)) };
            anim.Controller.States[i].EditorPos = { pos.x, pos.y };
        }

        if (ed::HasSelectionChanged())
        {
            ed::NodeId selNode;
            ed::LinkId selLink;
            bool hasNode { ed::GetSelectedNodes(&selNode, 1) > 0 };
            bool hasLink { ed::GetSelectedLinks(&selLink, 1) > 0 };

            if (hasNode)
            {
                uint32_t nid { static_cast<uint32_t>(selNode.Get()) };
                m_SelectedLink = UINT32_MAX;
                
                if (nid >= 10)
                {
                    uint32_t si { (nid - 10) / 3 };

                    if (si < anim.Controller.States.size())
                    {
                        if (m_SelectedState != si) { m_PreviewTime = 0.0f; }

                        m_SelectedState = si;
                    }
                    else { m_SelectedState = UINT32_MAX; }
                }
                else { m_SelectedState = UINT32_MAX; }
            }
            else if (hasLink)
            {
                uint32_t lid { static_cast<uint32_t>(selLink.Get()) };
                m_SelectedLink = (lid >= 10000) ? lid - 10000 : UINT32_MAX;
                m_SelectedState = UINT32_MAX;
            }
            else
            {
                m_SelectedState = UINT32_MAX;
                m_SelectedLink = UINT32_MAX;
            }
        }

        ed::SetCurrentEditor(nullptr);
    }

    void AnimatorPanel::DrawInspectorPanel(AnimatorComponent& anim)
    {
        if (m_SelectedLink != UINT32_MAX && m_SelectedLink < anim.Controller.Transitions.size())
        {
            auto& tr { anim.Controller.Transitions[m_SelectedLink] };

            auto stateName = [&](uint32_t idx) -> const char*
            {
                if (idx == AnimationTransition::ENTRY) { return "Entry"; }
                return idx < anim.Controller.States.size() ? anim.Controller.States[idx].Name.c_str() : "?";
            };

            ImGui::Text("%s  ->  %s", stateName(tr.FromState), stateName(tr.ToState));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::DragFloat("Blend##b", &tr.BlendDuration, 0.01f, 0.0f, 2.0f, "%.2fs");
            ImGui::SameLine();
            ImGui::Checkbox("Exit##e", &tr.HasExitTime);

            if (tr.HasExitTime)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                ImGui::DragFloat("##et", &tr.ExitTime, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Conditions");

            uint32_t condDelete { UINT32_MAX };

            for (uint32_t ci { 0 }; ci < static_cast<uint32_t>(tr.Conditions.size()); ++ci)
            {
                auto& cond { tr.Conditions[ci] };
                ImGui::PushID(static_cast<int>(ci));

                const char* pname { cond.ParameterIndex < anim.Controller.Parameters.size()
                    ? anim.Controller.Parameters[cond.ParameterIndex].Name.c_str() : "???" };
                ImGui::SetNextItemWidth(85.0f);

                if (ImGui::BeginCombo("##cp", pname))
                {
                    for (uint32_t pi { 0 }; pi < static_cast<uint32_t>(anim.Controller.Parameters.size()); ++pi)
                    {
                        bool sel { cond.ParameterIndex == pi };
                        if (ImGui::Selectable(anim.Controller.Parameters[pi].Name.c_str(), sel))
                        {
                            cond.ParameterIndex = pi;
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::SameLine();

                AnimatorParameter::Type ptype {
                    cond.ParameterIndex < anim.Controller.Parameters.size()
                        ? anim.Controller.Parameters[cond.ParameterIndex].ParamType
                        : AnimatorParameter::Type::Float };

                bool isBoolLike { ptype == AnimatorParameter::Type::Bool || ptype == AnimatorParameter::Type::Trigger };

                if (isBoolLike)
                {
                    const char* bops[] { "True", "False" };
                    int opIdx { cond.Operation == TransitionCondition::Op::False ? 1 : 0 };
                    ImGui::SetNextItemWidth(60.0f);
                    if (ImGui::Combo("##co", &opIdx, bops, 2))
                    {
                        cond.Operation = opIdx == 1 ? TransitionCondition::Op::False : TransitionCondition::Op::True;
                    }
                }
                else
                {
                    const char* nops[] { ">", "<", "==", "!=" };
                    int opIdx { 0 };

                    switch (cond.Operation)
                    {
                        case TransitionCondition::Op::Less:     opIdx = 1; break;
                        case TransitionCondition::Op::Equal:    opIdx = 2; break;
                        case TransitionCondition::Op::NotEqual: opIdx = 3; break;
                        default: break;
                    }

                    ImGui::SetNextItemWidth(46.0f);

                    if (ImGui::Combo("##co", &opIdx, nops, 4))
                    {
                        switch (opIdx)
                        {
                            case 0: cond.Operation = TransitionCondition::Op::Greater; break;
                            case 1: cond.Operation = TransitionCondition::Op::Less; break;
                            case 2: cond.Operation = TransitionCondition::Op::Equal; break;
                            case 3: cond.Operation = TransitionCondition::Op::NotEqual; break;
                        }
                    }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::DragFloat("##cv", &cond.Threshold, 0.1f, 0.0f, 0.0f, "%.1f");
                }

                ImGui::SameLine();

                if (ImGui::SmallButton("x")) { condDelete = ci; }

                ImGui::PopID();
            }

            if (condDelete != UINT32_MAX)
            {
                tr.Conditions.erase(tr.Conditions.begin() + condDelete);
            }

            if (!anim.Controller.Parameters.empty())
            {
                if (ImGui::SmallButton("+ Condition"))
                {
                    TransitionCondition c;
                    c.ParameterIndex = 0;
                    bool bl { anim.Controller.Parameters[0].ParamType == AnimatorParameter::Type::Bool ||
                              anim.Controller.Parameters[0].ParamType == AnimatorParameter::Type::Trigger };
                    c.Operation = bl ? TransitionCondition::Op::True : TransitionCondition::Op::Greater;
                    tr.Conditions.push_back(c);
                }
            }

            return;
        }

        if (m_SelectedState != UINT32_MAX && m_SelectedState < anim.Controller.States.size())
        {
            auto& state { anim.Controller.States[m_SelectedState] };

            ImGui::Text("%s", state.Name.c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(55.0f);
            ImGui::DragFloat("Speed##st", &state.Speed, 0.01f, 0.01f, 10.0f, "%.2f");
            ImGui::SameLine();
            ImGui::Checkbox("Loop##st", &state.Loop);
            ImGui::Separator();

            bool hasClip { state.ClipIndex < anim.Controller.Clips.size() };
            float bw { ImGui::GetContentRegionAvail().x };
            const char* label { hasClip ? anim.Controller.Clips[state.ClipIndex].Name.c_str() : "[ drag clip here ]" };
            ImGui::Button(label, ImVec2(bw, 0.0f));

            if (ImGui::BeginDragDropTarget())
            {
                if (auto* payload { ImGui::AcceptDragDropPayload("ANIM_CLIP") })
                {
                    const auto* dp { reinterpret_cast<const ClipDragPayload*>(payload->Data) };

                    if (m_ModelCache)
                    {
                        auto it { m_ModelCache->find((uint64_t)dp->ModelAssetUUID) };

                        if (it != m_ModelCache->end() && dp->ClipIndex < it->second.Animations.size())
                        {
                            const auto& src { it->second.Animations[dp->ClipIndex] };
                            uint32_t ci { UINT32_MAX };

                            for (uint32_t c { 0 }; c < static_cast<uint32_t>(anim.Controller.Clips.size()); ++c)
                            {
                                if (anim.Controller.Clips[c].Name == src.Name) { ci = c; break; }
                            }

                            if (ci == UINT32_MAX)
                            {
                                ci = static_cast<uint32_t>(anim.Controller.Clips.size());
                                anim.Controller.Clips.push_back(src);
                            }

                            state.ClipIndex = ci;
                            m_PreviewTime = 0.0f;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (hasClip)
            {
                const auto& clip { anim.Controller.Clips[state.ClipIndex] };
                float dur { clip.TicksPerSecond > 0.0f ? clip.Duration / clip.TicksPerSecond : 0.0f };

                m_PreviewTime += ImGui::GetIO().DeltaTime * state.Speed * anim.Speed;

                if (dur > 0.0f) { m_PreviewTime = std::fmod(m_PreviewTime, dur); }

                ImGui::Spacing();
                ImGui::ProgressBar(dur > 0.0f ? m_PreviewTime / dur : 0.0f, ImVec2(-1.0f, 8.0f), "");
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.2f / %.2fs", m_PreviewTime, dur);
                ImGui::TextDisabled("%s", buf);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Events");

            uint32_t evtDelete { UINT32_MAX };

            for (uint32_t ei { 0 }; ei < static_cast<uint32_t>(state.Events.size()); ++ei)
            {
                auto& evt { state.Events[ei] };
                ImGui::PushID(static_cast<int>(ei));

                char nameBuf[64] {};
                std::strncpy(nameBuf, evt.Name.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
                if (ImGui::InputText("##en", nameBuf, sizeof(nameBuf))) { evt.Name = nameBuf; }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60.0f);
                ImGui::DragFloat("##et", &evt.NormalizedTime, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) { evtDelete = ei; }

                ImGui::PopID();
            }

            if (evtDelete != UINT32_MAX) { state.Events.erase(state.Events.begin() + evtDelete); }
            if (ImGui::SmallButton("+ Event")) { state.Events.push_back({ "OnEvent", 0.5f }); }
        }
    }
}