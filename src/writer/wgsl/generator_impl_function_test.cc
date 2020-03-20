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

#include "gtest/gtest.h"
#include "src/ast/function.h"
#include "src/ast/kill_statement.h"
#include "src/ast/return_statement.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/void_type.h"
#include "src/ast/variable.h"
#include "src/writer/wgsl/generator_impl.h"

namespace tint {
namespace writer {
namespace wgsl {
namespace {

using GeneratorImplTest = testing::Test;

TEST_F(GeneratorImplTest, Emit_Function) {
  std::vector<std::unique_ptr<ast::Statement>> body;
  body.push_back(std::make_unique<ast::KillStatement>());
  body.push_back(std::make_unique<ast::ReturnStatement>());

  ast::type::VoidType void_type;
  ast::Function func("my_func", {}, &void_type);
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  fn my_func() -> void {
    kill;
    return;
  }
)");
}

TEST_F(GeneratorImplTest, Emit_Function_WithParams) {
  std::vector<std::unique_ptr<ast::Statement>> body;
  body.push_back(std::make_unique<ast::KillStatement>());
  body.push_back(std::make_unique<ast::ReturnStatement>());

  ast::type::F32Type f32;
  ast::type::I32Type i32;
  std::vector<std::unique_ptr<ast::Variable>> params;
  params.push_back(
      std::make_unique<ast::Variable>("a", ast::StorageClass::kNone, &f32));
  params.push_back(
      std::make_unique<ast::Variable>("b", ast::StorageClass::kNone, &i32));

  ast::type::VoidType void_type;
  ast::Function func("my_func", std::move(params), &void_type);
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  fn my_func(a : f32, b : i32) -> void {
    kill;
    return;
  }
)");
}

}  // namespace
}  // namespace wgsl
}  // namespace writer
}  // namespace tint
