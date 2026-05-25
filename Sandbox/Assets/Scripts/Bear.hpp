#pragma once

#include <AntelopeScript.hpp>


ANTELOPE_SCRIPT()
class Bear : public Antelope::Script
{
    public:
        void OnCreate() override;
        void OnUpdate(float dt) override;
        void OnDestroy() override;

    private:
        bool m_WasXPressed { false };
};