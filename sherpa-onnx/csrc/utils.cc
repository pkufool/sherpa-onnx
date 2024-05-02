// sherpa-onnx/csrc/utils.cc
//
// Copyright      2023  Xiaomi Corporation

#include "sherpa-onnx/csrc/utils.h"

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "sherpa-onnx/csrc/log.h"
#include "sherpa-onnx/csrc/macros.h"
#include "sherpa-onnx/csrc/text-utils.h"

namespace sherpa_onnx {

static bool EncodeBase(std::vector<std::string> &lines,
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

  for (const auto &line : lines) {
    float score = 0;
    float threshold = 0;
    std::string phrase = "";

    std::istringstream iss(line);
    while (iss >> word) {
      if (word.size() >= 3) {
        // For BPE-based models, we replace ▁ with a space
        // Unicode 9601, hex 0x2581, utf8 0xe29681
        const uint8_t *p = reinterpret_cast<const uint8_t *>(word.c_str());
        if (p[0] == 0xe2 && p[1] == 0x96 && p[2] == 0x81) {
          word = word.replace(0, 3, " ");
        }
      }
      if (symbol_table.contains(word)) {
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
            has_phrases = true;
            break;
          default:
            SHERPA_ONNX_LOGE(
                "Cannot find ID for token %s at line: %s. (Hint: words on "
                "the same line are separated by spaces)",
                word.c_str(), line.c_str());
            return false;
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
  return true;
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
        SHERPA_ONNX_CHECK_EQ(modeling_unit, "cjkchar+bpe");
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
                    std::vector<std::vector<int32_t>> *keywords_id,
                    std::vector<std::string> *keywords,
                    std::vector<float> *boost_scores,
                    std::vector<float> *threshold) {
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(is, line)) {
    lines.push_back(line);
  }
  return EncodeBase(lines, symbol_table, keywords_id, keywords, boost_scores,
                    threshold);
}

}  // namespace sherpa_onnx
