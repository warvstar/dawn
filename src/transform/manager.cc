// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/transform/manager.h"

#include "src/type_determiner.h"

namespace tint {
namespace transform {

Manager::Manager() = default;
Manager::~Manager() = default;

Transform::Output Manager::Run(ast::Module* module) {
  Output out;
  for (auto& transform : transforms_) {
    auto res = transform->Run(module);
    out.module = std::move(res.module);
    out.diagnostics.add(std::move(res.diagnostics));
    if (out.diagnostics.contains_errors()) {
      return out;
    }
    module = &out.module;
  }

  TypeDeterminer td(module);
  if (!td.Determine()) {
    diag::Diagnostic err;
    err.severity = diag::Severity::Error;
    err.message = td.error();
    out.diagnostics.add(std::move(err));
  }

  return out;
}

}  // namespace transform
}  // namespace tint
