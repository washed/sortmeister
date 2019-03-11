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
#include <chrono>

#include <boost/iostreams/device/mapped_file.hpp>

using namespace std;
namespace fs = std::experimental::filesystem;

const std::vector<char> alphabet_without_symbol{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  //
                                                 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',  //
                                                 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',  //
                                                 'u', 'v', 'w', 'x', 'y', 'z' };

const std::vector<char> alphabet_with_symbol{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  //
                                              'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',  //
                                              'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',  //
                                              'u', 'v', 'w', 'x', 'y', 'z', '*' };

constexpr char symbol_regex[] = "[^A-Za-z0-9]";

std::string target_delimiter{ ":" };
std::vector<std::string> delimiters{ target_delimiter, ";", "\t", " " };

const fs::path temp_file_suffix_path{ "_temp" };

// Adapted from here:
// https://stackoverflow.com/questions/2380962/generate-all-combinations-of-arbitrary-alphabet-up-to-arbitrary-length
template <size_t length>
constexpr std::vector<std::string> get_all_words( const std::vector<char> alphabet )
{
  std::vector<int> index( length, 0 );
  std::vector<std::string> words;

  while ( true )
  {
    char word[ length + 1 ] = "";
    for ( int i = 0; i < length; ++i )
    {
      word[ i ] = alphabet[ index[ i ] ];
    }
    words.push_back( word );

    for ( int i = length - 1;; --i )
    {
      if ( i < 0 )
      {
        return words;
      }
      index[ i ]++;
      if ( index[ i ] == alphabet.size() )
      {
        index[ i ] = 0;
      }
      else
      {
        break;
      }
    }
  }
}

void build_file_vector( const fs::path& pathToShow, std::vector<fs::path>& files )
{
  if ( fs::exists( pathToShow ) && fs::is_directory( pathToShow ) )
  {
    for ( const auto& entry : fs::directory_iterator( pathToShow ) )
    {
      auto filename = entry.path().filename();
      if ( fs::is_directory( entry.status() ) )
      {
        build_file_vector( entry, files );
      }
      else if ( fs::is_regular_file( entry.status() ) )
      {
        files.emplace_back( entry );
      }
      else
      {
        cout << " [?]" << filename << "\n";
      }
    }
  }
}

void get_all_files( const fs::path& path_to_search, std::vector<fs::path>& files )
{
  build_file_vector( path_to_search, files );
}

void uniquify_in_files( const fs::path& path_to_search )
{
  std::vector<fs::path> files;
  get_all_files( path_to_search, files );

  for ( const auto& file : files )
  {
    std::cout << "Conforming file: " << file << " ...";

    std::fstream input_file{ file, ios::in };
    std::vector<std::string> x( std::istream_iterator<std::string>( input_file ), {} );
    input_file.close();

    std::sort( x.begin(), x.end() );
    x.erase( unique( x.begin(), x.end() ), x.end() );

    fs::path out_temp_file_path( file.parent_path() );
    out_temp_file_path /= file.filename() += "_temp";

    std::fstream output_file{ out_temp_file_path, ios::out };

    for ( const auto& entry : x )
    {
      output_file << entry << '\n';
    }
    output_file.close();

    fs::remove( file );
    fs::rename( out_temp_file_path, file );

    std::cout << " done!\n";
  }
}

template <size_t match_char_count>
bool a_less_b_nchars( const std::string& a, const std::string& b )
{
  auto compare_length = std::min( match_char_count, std::min( a.size(), b.size() ) );

  return std::lexicographical_compare( a.begin(), a.begin() + compare_length, b.begin(), b.begin() + compare_length );
}

void conform_entry( std::string& a )
{
  boost::trim( a );

  // TODO: Find email and password part! (this could go together with the
  // delimiter stuff!)

  // This isn't really good
  for ( const auto& delimiter : delimiters )
  {
    size_t delimiter_pos = a.find( delimiter, 0 );
    if ( delimiter_pos != std::string::npos )
    {
      if ( delimiter != target_delimiter )
      {
        a[ delimiter_pos ] = target_delimiter.c_str()[ 0 ];
      }
      std::transform( a.begin(), a.begin() + delimiter_pos, a.begin(), ::tolower );
      return;
    }
  }
}

fs::path path_from_word( const std::string& word )
{
  fs::path path;
  for ( const auto& character : word )
  {
    if ( character == '*' )
    {
      path.append( "symbol" );
    }
    else
    {
      path.append( std::string{ character } );
    }
  }
  return path;
}

std::vector<fs::path> folder_per_char( const std::vector<std::string>& words )
{
  std::vector<fs::path> paths;
  for ( const auto& word : words )
  {
    paths.emplace_back( path_from_word( word ) );
  }
  return paths;
}

void create_output_folders( const fs::path& root, const std::vector<std::string>& words )
{
  auto test = folder_per_char( words );
  for ( const auto& elem : test )
  {
    fs::create_directories( fs::path{ root / elem } );
  }
}

void vectorize_lines( const fs::path& file, std::vector<std::string>& str_vec )
{
  boost::iostreams::mapped_file mmap( file.string(), boost::iostreams::mapped_file::readonly );
  auto line_start = mmap.const_data();
  auto file_end = line_start + mmap.size();
  auto line_end = static_cast<const char*>( memchr( line_start, '\n', file_end - line_start ) );

  while ( line_start && line_start != file_end )
  {
    str_vec.push_back( std::string( line_start, line_end - line_start ) );
    line_start = line_end + 1;
    line_end = static_cast<const char*>( memchr( line_start, '\n', file_end - line_start ) );
  }
}

int main()
{
  constexpr size_t pre_sort_depth = 3;
  auto words_without_symbols = get_all_words<pre_sort_depth>( alphabet_without_symbol );
  auto words_with_symbols = get_all_words<pre_sort_depth>( alphabet_with_symbol );
  std::sort( words_without_symbols.begin(), words_without_symbols.end() );
  std::sort( words_with_symbols.begin(), words_with_symbols.end() );

  std::vector<std::string> words_only_symbols;
  std::set_difference( words_with_symbols.begin(), words_with_symbols.end(), words_without_symbols.begin(),
                       words_without_symbols.end(), std::back_inserter( words_only_symbols ) );

  std::vector<std::string> words;
  words.insert( words.begin(), words_without_symbols.begin(), words_without_symbols.end() );
  words.insert( words.end(), words_only_symbols.begin(), words_only_symbols.end() );

  const fs::path input_dir_path{ "C:\\Projects\\other\\sortmeister\\data\\in" };
  const fs::path output_dir_path{ "C:\\Projects\\other\\sortmeister\\data\\out" };

  size_t total_matched_count = 0;
  size_t total_unmatched_count = 0;

  std::vector<fs::path> files;
  get_all_files( input_dir_path, files );

  for ( const auto& file : files )
  {
    std::cout << "Reading and inserting file: " << file << " ...";

    std::cout << "Reading: " << file << " ...";
    auto begin = std::chrono::steady_clock::now();

    std::vector<std::string> input;
    input.reserve( 100000000 );
    vectorize_lines( file, input );

    auto end = std::chrono::steady_clock::now();
    std::cout << " done! Took " << std::chrono::duration_cast<std::chrono::milliseconds>( end - begin ).count()
              << " ms for " << input.size() << " lines.\n";

    // Conform the new entries
    std::cout << "Conforming " << input.size() << " entries.\n";
    begin = std::chrono::steady_clock::now();

    std::for_each( input.begin(), input.end(), conform_entry );
    std::sort( input.begin(), input.end() );
    input.erase( unique( input.begin(), input.end() ), input.end() );

    end = std::chrono::steady_clock::now();
    std::cout << "Took " << std::chrono::duration_cast<std::chrono::milliseconds>( end - begin ).count() << " ms. "
              << input.size() << " lines remain.\n";

    for ( const auto& word : words )
    {
      vector<string> matches;
      vector<string>::iterator lower, upper;

      if ( word.find( "*" ) == std::string::npos )
      {
        lower = lower_bound( input.begin(), input.end(), word, a_less_b_nchars<pre_sort_depth> );
        upper = upper_bound( lower, input.end(), word, a_less_b_nchars<pre_sort_depth> );

        matches.insert( matches.begin(), lower, upper );
        input.erase( lower, upper );
      }
      else
      {
        // Pattern contains a wild-card

        // symbol_regex
        std::string regex_string = "^";
        for ( const auto& character : word )
        {
          if ( character != '*' )
          {
            regex_string += character;
          }
          else
          {
            regex_string += symbol_regex;
          }
        }
        regex_string += ".*";

        std::regex wildcard_regex( regex_string );
        std::smatch wildcard_match;

        std::vector<std::string> input_erase;
        for ( auto it = input.begin(); it != input.end(); ++it )
        {
          if ( std::regex_search( *it, wildcard_match, wildcard_regex ) )
          {
            matches.emplace_back( wildcard_match.str() );
            input_erase.emplace_back( *it );
          }
        }

        std::vector<std::string> new_input;
        std::set_difference( input.begin(), input.end(), input_erase.begin(), input_erase.end(),
                             std::back_inserter( new_input ) );

        input = new_input;
      }

      if ( matches.size() != 0 )
      {
        // Read its contents
        fs::path merge_in_file_path{ output_dir_path / path_from_word( word ) };

        if ( fs::exists( merge_in_file_path ) )
        {
          // Merge matches
          std::cout << "Merging " << matches.size() << " entries into " << merge_in_file_path;
          std::vector<std::string> merge_in;
          merge_in.reserve( 10000000 );  // TODO: Handle these reservations better
          vectorize_lines( merge_in_file_path, merge_in );

          // Both the file and the input vector must be sorted! Merge them:
          std::vector<std::string> merge_out;
          merge_out.reserve( matches.size() + merge_in.size() );
          std::set_union( merge_in.begin(), merge_in.end(), matches.begin(), matches.end(),
                          std::back_inserter( merge_out ) );

          auto delta = merge_out.size() - merge_in.size();
          std::cout << " Delta: " << ( delta ) << '\n';

          if ( delta > 0 )
          {
            // Output file
            fs::path merge_out_file_path{ merge_in_file_path };
            merge_out_file_path += temp_file_suffix_path;

            // TODO: Better way to write to the output file?
            std::ofstream merge_out_file{ merge_out_file_path, ios::out | ios::binary };
            for ( const auto& entry : merge_out )
            {
              merge_out_file << entry << '\n';
            }
            merge_out_file.close();

            // Delete merge_in, rename merge_out
            fs::remove( merge_in_file_path );
            fs::rename( merge_out_file_path, merge_in_file_path );
          }
        }
        else
        {
          fs::path direct_out_file_path{ merge_in_file_path };
          fs::create_directories( direct_out_file_path.parent_path() );

          // Write matches directly
          std::cout << "Writing " << matches.size() << " entries to " << direct_out_file_path << '\n';

          std::ofstream direct_out_file{ direct_out_file_path, ios::out | ios::binary };
          for ( const auto& entry : matches )
          {
            direct_out_file << entry << '\n';
          }
          direct_out_file.close();
        }

        total_matched_count += matches.size();
      }
    }

    std::time_t current_time = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
    std::cout << "Now: " << current_time << '\n';
    std::stringstream bla;
    bla << current_time;

    total_unmatched_count += input.size();
    fs::path unmatched_out_file_path{ output_dir_path };
    unmatched_out_file_path /= "unmatched_";
    unmatched_out_file_path += bla.str();
    std::ofstream unmatched_out_file{ unmatched_out_file_path, ios::out | ios::app };
    for ( const auto& entry : input )
    {
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
 +   Implement automatic batch execution for small files (load up to a certain
 amount of entries and then merge, instead merge every single input file)
 +   Write fast search engine
 +   Improve conforming filters, put filtered stuff in a trash file
 +   Analyze behaviour seen during pre-sorted import: There seems to be always
 at least 1 entry being merged in to all output files. If this is true, previous
 was bad, or we have a bug here.
 +   Add some statistics about read entries, unique entries in input, unique in
 +   Do the basic string filtering directly at file read
 +   Improve filtering
 +   Consider replacing getline if something else is faster
 +   Add progress indicators where possible
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
