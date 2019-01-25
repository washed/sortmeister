// ConsoleApplication1.cpp : This file contains the 'main' function. Program
// execution begins and ends there.
//

#include "pch.h"
//
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <ctime>
#include <experimental/filesystem>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::experimental::filesystem;

std::vector<char> alphabet{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  //
                           'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',  //
                           'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',  //
                           'u', 'v', 'w', 'x', 'y', 'z', '*'};

constexpr char symbol_regex[] = "[^A-Za-z0-9]";

std::string target_delimiter{":"};
std::vector<std::string> delimiters{target_delimiter, ";", " "};

const fs::path temp_file_suffix_path{"_temp"};

// Adapted from here:
// https://stackoverflow.com/questions/2380962/generate-all-combinations-of-arbitrary-alphabet-up-to-arbitrary-length
template <size_t length>
constexpr std::vector<std::string> get_all_words() {
  std::vector<int> index(length, 0);
  std::vector<std::string> words;

  while (true) {
    char word[length + 1] = "";
    for (int i = 0; i < length; ++i) {
      word[i] = alphabet[index[i]];
    }
    words.push_back(word);

    for (int i = length - 1;; --i) {
      if (i < 0) {
        return words;
      }
      index[i]++;
      if (index[i] == alphabet.size()) {
        index[i] = 0;
      } else {
        break;
      }
    }
  }
}

void build_file_vector(const fs::path& pathToShow,
                       std::vector<fs::path>& files) {
  if (fs::exists(pathToShow) && fs::is_directory(pathToShow)) {
    for (const auto& entry : fs::directory_iterator(pathToShow)) {
      auto filename = entry.path().filename();
      if (fs::is_directory(entry.status())) {
        build_file_vector(entry, files);
      } else if (fs::is_regular_file(entry.status())) {
        files.emplace_back(entry);
      } else {
        cout << " [?]" << filename << "\n";
      }
    }
  }
}

void get_all_files(const fs::path& path_to_search,
                   std::vector<fs::path>& files) {
  build_file_vector(path_to_search, files);
}

bool a_less_b_first_char(const std::string& a, const std::string& b) {
  return (a[0] < b[0]);
}

bool a_less_b_second_char(const std::string& a, const std::string& b) {
  return (a[1] < b[1]);
}

bool a_less_b_first_two_chars(const std::string& a, const std::string& b) {
  if (a[0] < b[0]) {
    return true;
  } else if (a[0] > b[0]) {
    return false;
  } else {
    return (a[1] < b[1]);
  }
}

void uniquify_in_files(const fs::path& path_to_search) {
  std::vector<fs::path> files;
  get_all_files(path_to_search, files);

  for (const auto& file : files) {
    std::cout << "Conforming file: " << file << " ...";

    std::fstream input_file{file, ios::in};
    std::vector<std::string> x(std::istream_iterator<std::string>(input_file),
                               {});
    input_file.close();

    std::sort(x.begin(), x.end());
    x.erase(unique(x.begin(), x.end()), x.end());

    fs::path out_temp_file_path(file.parent_path());
    out_temp_file_path /= file.filename() += "_temp";

    std::fstream output_file{out_temp_file_path, ios::out};

    for (const auto& entry : x) {
      output_file << entry << '\n';
    }
    output_file.close();

    fs::remove(file);
    fs::rename(out_temp_file_path, file);

    std::cout << " done!\n";
  }
}

class tree_node {
 public:
  tree_node() {}
  ~tree_node() {}

 private:
  const char* _name;

  tree_node* _parent;
  std::vector<tree_node*> _children;
};

template <size_t match_char_count>
bool a_less_b_nchars(const std::string& a, const std::string& b) {
  auto compare_length =
      std::min(match_char_count, std::min(a.size(), b.size()));

  return std::lexicographical_compare(a.begin(), a.begin() + compare_length,
                                      b.begin(), b.begin() + compare_length);
}

void conform_entry(std::string& a) {
  boost::trim(a);
  // TODO: Find email and password part! (this could go together with the
  // delimiter stuff!)

  // This isn't really good
  for (const auto& delimiter : delimiters) {
    size_t delimiter_pos = a.find(delimiter, 0);
    if (delimiter_pos != std::string::npos) {
      if (delimiter != target_delimiter) {
        a[delimiter_pos] = target_delimiter.c_str()[0];
      }
      std::transform(a.begin(), a.begin() + delimiter_pos, a.begin(),
                     ::tolower);
    }
  }
}

fs::path path_from_word(const std::string& word) {
  fs::path path;
  for (const auto& character : word) {
    if (character == '*') {
      path.append("symbol");
    } else {
      path.append(std::string{character});
    }
  }
  return path;
}

std::vector<fs::path> folder_per_char(const std::vector<std::string>& words) {
  std::vector<fs::path> paths;
  for (const auto& word : words) {
    paths.emplace_back(path_from_word(word));
  }
  return paths;
}

void create_output_folders(const fs::path& root,
                           const std::vector<std::string>& words) {
  auto test = folder_per_char(words);
  for (const auto& elem : test) {
    fs::create_directories(fs::path{root / elem});
  }
}

int main() {
  constexpr size_t pre_sort_depth = 3;
  auto words = get_all_words<pre_sort_depth>();
  std::sort(words.begin(), words.end());

  const fs::path input_dir_path{""};
  const fs::path output_dir_path{""};

  size_t total_matched_count = 0;
  size_t total_unmatched_count = 0;

  std::vector<fs::path> files;
  get_all_files(input_dir_path, files);

  for (const auto& file : files) {
    std::cout << "Reading and inserting file: " << file << " ...";

    // Read into temp vector
    std::fstream input_file{file, ios::in};
    std::vector<std::string> input(
        std::istream_iterator<std::string>(input_file), {});
    input_file.close();

    std::cout << " done!" << '\n';

    std::cout << "Read " << input.size() << " lines! Starting cleanup... "
              << '\n';

    // Conform the new entries
    std::for_each(input.begin(), input.end(), conform_entry);
    std::sort(input.begin(), input.end());
    input.erase(unique(input.begin(), input.end()), input.end());

    std::cout << " done!" << '\n'
              << "Now " << input.size() << " lines left!" << '\n';

    for (const auto& word : words) {
      vector<string> matches;
      vector<string>::iterator lower, upper;

      if (word.find("*") == std::string::npos) {
        lower = lower_bound(input.begin(), input.end(), word,
                            a_less_b_nchars<pre_sort_depth>);
        upper = upper_bound(input.begin(), input.end(), word,
                            a_less_b_nchars<pre_sort_depth>);

        matches.insert(matches.begin(), lower, upper);
        input.erase(lower, upper);
      }
      /*
      else {
        // Pattern contains a wild-card

        // symbol_regex
        std::string regex_string = "^";
        for (const auto& character : word) {
          if (character != '*') {
            regex_string += character;
          } else {
            regex_string += symbol_regex;
          }
        }
        regex_string += ".*";

        std::regex wildcard_regex(regex_string);
        std::smatch wildcard_match;

        vector<std::vector<std::string>::iterator> input_erase;
        for (auto it = input.begin(); it != input.end(); ++it) {
          if (std::regex_search(*it, wildcard_match, wildcard_regex)) {
            matches.emplace_back(wildcard_match.str());
            input_erase.emplace_back(it);
          }
        }

        // Delete matches in input
        for (auto it = input_erase.begin(); it != input_erase.end(); ++it) {
          input.erase(*it);
        }
      }
      */

      if (matches.size() != 0) {
        // Read its contents
        fs::path merge_in_file_path{output_dir_path / path_from_word(word)};

        if (fs::exists(merge_in_file_path)) {
          // Merge matches
          std::cout << "Merging " << matches.size() << " entries into "
                    << merge_in_file_path << '\n';

          std::fstream merge_in_file{merge_in_file_path, ios::in};
          std::vector<std::string> merge_in(
              std::istream_iterator<std::string>(merge_in_file), {});
          merge_in_file.close();

          // Both the file and the input vector must be sorted! Merge them:
          std::vector<std::string> merge_out;
          std::set_union(merge_in.begin(), merge_in.end(), matches.begin(),
                         matches.end(), std::back_inserter(merge_out));

          // Output file
          fs::path merge_out_file_path{merge_in_file_path};
          merge_out_file_path += temp_file_suffix_path;
          std::fstream merge_out_file{merge_out_file_path, ios::out};
          for (const auto& entry : merge_out) {
            merge_out_file << entry << '\n';
          }
          merge_out_file.close();

          // Delete merge_in, rename merge_out
          fs::remove(merge_in_file_path);
          fs::rename(merge_out_file_path, merge_in_file_path);
        } else {
          fs::path direct_out_file_path{merge_in_file_path};
          fs::create_directories(direct_out_file_path.parent_path());

          // Write matches directly
          std::cout << "Writing " << matches.size() << " entries to "
                    << direct_out_file_path << '\n';

          std::fstream direct_out_file{direct_out_file_path, ios::out};
          for (const auto& entry : matches) {
            direct_out_file << entry << '\n';
          }
          direct_out_file.close();
        }

        total_matched_count += matches.size();
      }
    }

    std::time_t current_time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "Now: " << current_time << '\n';
    std::stringstream bla;
    bla << current_time;

    total_unmatched_count += input.size();
    fs::path unmatched_out_file_path{output_dir_path};
    unmatched_out_file_path /= "unmatched_";
    unmatched_out_file_path += bla.str();
    std::fstream unmatched_out_file{unmatched_out_file_path,
                                    ios::out | ios::app};
    for (const auto& entry : input) {
      unmatched_out_file << entry << '\n';
    }
    unmatched_out_file.close();
  }
  std::cout << "Match success for " << total_matched_count << " entries!\n";
  std::cout << "Match failed for  " << total_unmatched_count << " entries!\n";

  std::cout << "All done!" << '\n';
}

// TODO:
/*
 +   Implement wildcard (/symbol) filter properly and without regex if possible
 +   Implement automatic batch execution for small files (load up to a certain amount of entries and then merge, instead merge every single input file)
 +   Write fast search engine
 +   Improve conforming filters, put filtered stuff in a trash file
 +   Analyze behaviour seen during pre-sorted import: There seems to be always at least 1 entry being merged in to all output files. If this is true, previous was bad, or we have a bug here.
 */

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add
//   Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project
//   and select the .sln file
