#pragma once

#include <Urho3D/Urho3DAll.h>

#include <gtest/gtest.h>

using namespace Urho3D;

class BaseTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        context_ = SharedPtr<Context>(new Context());
        context_->RegisterSubsystem(new FileSystem(context_));
        context_->RegisterSubsystem(new ResourceCache(context_));
        context_->RegisterSubsystem(new WorkQueue(context_));
        RegisterSceneLibrary(context_);
        RegisterGraphicsLibrary(context_);
        RegisterPhysicsLibrary(context_);
    }

    void TearDown() override
    {
    }

    SharedPtr<Context> context_;
};
