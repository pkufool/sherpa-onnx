// sherpa-onnx/csrc/context-graph-test.cc
//
// Copyright (c)  2023  Xiaomi Corporation

#include "sherpa-onnx/csrc/context-graph.h"

#include <map>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace sherpa_onnx {

TEST(ContextGraph, TestBasic) {
  std::vector<std::string> contexts_str(
      {"S", "HE", "SHE", "SHELL", "HIS", "HERS", "HELLO", "THIS", "THEM"});
  std::vector<std::vector<int32_t>> contexts;
  for (int32_t i = 0; i < contexts_str.size(); ++i) {
    std::vector<int32_t> tmp;
    for (int32_t j = 0; j < contexts_str[i].size(); ++j) {
      tmp.push_back((int32_t)contexts_str[i][j]);
    }
    contexts.push_back(tmp);
  }
  auto context_graph = ContextGraph(1);
  context_graph.Build(contexts);

  auto queries = std::map<std::string, float>{
      {"HEHERSHE", 14}, {"HERSHE", 12}, {"HISHE", 9},   {"SHED", 6},
      {"HELL", 2},      {"HELLO", 7},   {"DHRHISQ", 4}, {"THEN", 2}};

  for (auto iter : queries) {
    float total_scores = 0;
    auto state = context_graph.Root();
    for (auto q : iter.first) {
      auto res = context_graph.ForwardOneStep(state, (int32_t)q);
      total_scores += res.first;
      state = res.second;
    }
    auto res = context_graph.Finalize(state);
    EXPECT_EQ(res.second->token, -1);
    total_scores += res.first;
    EXPECT_EQ(total_scores, iter.second);
  }
}

}  // namespace sherpa_onnx