//
// Copyright (c) 2008-2018 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once


#include <Urho3D/Core/Object.h>
#include <cppast/cpp_entity.hpp>
#include <cppast/visitor.hpp>
#include "ParserPass.h"
#include "Utilities.h"


namespace Urho3D
{

/// Walk AST and gather known defined classes. Exclude protected/private members from generation.
class GatherInfoPass : public ParserPass
{
    URHO3D_OBJECT(GatherInfoPass, ParserPass);
public:
    explicit GatherInfoPass(Context* context) : ParserPass(context) { };

    void Start() override;
    void StartFile(const String& filePath) override;
    bool Visit(const cppast::cpp_entity& e, cppast::visitor_info info) override;

protected:
    cppast::cpp_access_specifier_kind access_ = cppast::cpp_private;
    IncludedChecker typeChecker_;
};

}
