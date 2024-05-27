// sherpa-onnx/csrc/utils.cc
//
// Copyright      2023  Xiaomi Corporation

#include "sherpa-onnx/csrc/utils.h"

#include <cassert>
#include <iostream>
#include <regex>  // NOLINT
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "sherpa-onnx/csrc/log.h"
#include "sherpa-onnx/csrc/macros.h"
#include "sherpa-onnx/csrc/text-utils.h"

namespace sherpa_onnx {

static bool EncodeBase(const std::vector<std::string> &lines,
                       const SymbolTable &symbol_table,
                       std::vector<std::vector<int32_t>> *ids,
                       std::vector<std::string> *phrases,
                       std::vector<float> *scores,
                       std::vector<float> *thresholds) {
  ids->clear();

  std::vector<int32_t> tmp_ids;
  std::vector<float> tmp_scores;
  std::vector<float> tmp_thresholds;
  std::vector<std::string> tmp_phrases;

  std::string line;
  std::string word;
  bool has_scores = false;
  bool has_thresholds = false;
  bool has_phrases = false;
  bool has_oov = false;

  for (const auto &line : lines) {
    float score = 0;
    float threshold = 0;
    std::string phrase = "";

    std::istringstream iss(line);
    while (iss >> word) {
      if (symbol_table.Contains(word)) {
        int32_t id = symbol_table[word];
        tmp_ids.push_back(id);
      } else {
        switch (word[0]) {
          case ':':  // boosting score for current keyword
            score = std::stof(word.substr(1));
            has_scores = true;
            break;
          case '#':  // triggering threshold (probability) for current keyword
            threshold = std::stof(word.substr(1));
            has_thresholds = true;
            break;
          case '@':  // the original keyword string
            phrase = word.substr(1);
            phrase = std::regex_replace(phrase, std::regex("_"), " ");
            has_phrases = true;
            break;
          default:
            SHERPA_ONNX_LOGE(
                "Cannot find ID for token %s at line: %s. (Hint: words on "
                "the same line are separated by spaces)",
                word.c_str(), line.c_str());
            has_oov = true;
            break;
        }
      }
    }
    ids->push_back(std::move(tmp_ids));
    tmp_scores.push_back(score);
    tmp_phrases.push_back(phrase);
    tmp_thresholds.push_back(threshold);
  }
  if (scores != nullptr) {
    if (has_scores) {
      scores->swap(tmp_scores);
    } else {
      scores->clear();
    }
  }
  if (phrases != nullptr) {
    if (has_phrases) {
      *phrases = std::move(tmp_phrases);
    } else {
      phrases->clear();
    }
  }
  if (thresholds != nullptr) {
    if (has_thresholds) {
      thresholds->swap(tmp_thresholds);
    } else {
      thresholds->clear();
    }
  }
  return !has_oov;
}

bool EncodeHotwords(std::istream &is, const std::string &modeling_unit,
                    const SymbolTable &symbol_table,
                    const ssentencepiece::Ssentencepiece *bpe_encoder,
                    std::vector<std::vector<int32_t>> *hotwords) {
  std::vector<std::string> lines;
  std::string line;
  std::string word;

  while (std::getline(is, line)) {
    std::string score;
    std::string phrase;

    std::ostringstream oss;
    std::istringstream iss(line);
    while (iss >> word) {
      switch (word[0]) {
        case ':':  // boosting score for current keyword
          score = word;
          break;
        default:
          if (!score.empty()) {
            SHERPA_ONNX_LOGE(
                "Boosting score should be put after the words/phrase, given "
                "%s.",
                line.c_str());
            return false;
          }
          oss << " " << word;
          break;
      }
    }
    phrase = oss.str().substr(1);
    std::istringstream piss(phrase);
    oss.clear();
    oss.str("");
    while (piss >> word) {
      if (modeling_unit == "cjkchar") {
        for (const auto &w : SplitUtf8(word)) {
          oss << " " << w;
        }
      } else if (modeling_unit == "bpe") {
        std::vector<std::string> bpes;
        bpe_encoder->Encode(word, &bpes);
        for (const auto &bpe : bpes) {
          oss << " " << bpe;
        }
      } else {
        if (modeling_unit != "cjkchar+bpe") {
          SHERPA_ONNX_LOGE(
              "modeling_unit should be one of bpe, cjkchar or cjkchar+bpe, "
              "given "
              "%s",
              modeling_unit.c_str());
          exit(-1);
        }
        for (const auto &w : SplitUtf8(word)) {
          if (isalpha(w[0])) {
            std::vector<std::string> bpes;
            bpe_encoder->Encode(w, &bpes);
            for (const auto &bpe : bpes) {
              oss << " " << bpe;
            }
          } else {
            oss << " " << w;
          }
        }
      }
    }
    std::string encoded_phrase = oss.str().substr(1);
    oss.clear();
    oss.str("");
    oss << encoded_phrase;
    if (!score.empty()) {
      oss << " " << score;
    }
    lines.push_back(oss.str());
  }
  return EncodeBase(lines, symbol_table, hotwords, nullptr, nullptr, nullptr);
}

bool EncodeKeywords(std::istream &is, const SymbolTable &symbol_table,
                    const ssentencepiece::Ssentencepiece *bpe_encoder,
                    const cppinyin::PinyinEncoder *pinyin_encoder,
                    std::vector<std::vector<int32_t>> *keywords_id,
                    std::vector<std::string> *keywords,
                    std::vector<float> *boost_scores,
                    std::vector<float> *threshold) {
  std::vector<std::string> lines;
  std::string line;
  std::string word;

  while (std::getline(is, line)) {
    std::string score;
    std::string phrase;
    std::string threshold;
    std::string custom_phrase;

    std::ostringstream oss;
    std::istringstream iss(line);
    while (iss >> word) {
      switch (word[0]) {
        case ':':  // boosting score for current keyword
          score = word;
          break;
        case '#':  // triggering threshold for current keyword
          threshold = word;
          break;
        case '@':  // the customize phrase for current keyword
          custom_phrase = word;
          break;
        default:
          if (!score.empty() || !threshold.empty() || !custom_phrase.empty()) {
            SHERPA_ONNX_LOGE(
                "Boosting score, threshold and customize phrase should be put "
                "after the words/phrase, given %s.",
                line.c_str());
            return false;
          }
          oss << " " << word;
          break;
      }
    }
    phrase = oss.str().substr(1);
    std::istringstream piss(phrase);
    oss.clear();
    oss.str("");
    std::ostringstream poss;
    while (piss >> word) {
      poss << "_" << word;
      if (modeling_unit == "bpe") {
        std::vector<std::string> bpes;
        bpe_encoder->Encode(word, &bpes);
        for (const auto &bpe : bpes) {
          oss << " " << bpe;
        }
      } else {
        if (modeling_unit != "ppinyin") {
          SHERPA_ONNX_LOGE(
              "modeling_unit should be one of bpe, ppinyin, "
              "given "
              "%s",
              modeling_unit.c_str());
          exit(-1);
        }
        std::vector<std::string> pinyins;
        pinyin_encoder->Encode(word, &pinyins);
        for (const auto &pinyin : pinyins) {
          oss << " " << pinyin;
        }
      }
    }
    std::string encoded_phrase = oss.str().substr(1);
    oss.clear();
    oss.str("");
    oss << encoded_phrase;
    if (!score.empty()) {
      oss << " " << score;
    }
    if (!threshold.empty()) {
      oss << " " << threshold;
    }
    if (!custom_phrase.empty()) {
      oss << " " << custom_phrase;
    } else {
      oss << " @" << poss.str().substr(1);
    }
    lines.push_back(oss.str());
  }

  return EncodeBase(lines, symbol_table, keywords_id, keywords, boost_scores,
                    threshold);
}

}  // namespace sherpa_onnx
