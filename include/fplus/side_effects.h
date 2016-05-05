// Copyright Tobias Hermann 2015.
// https://github.com/Dobiasd/FunctionalPlus
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "container_common.h"
#include "function_traits.h"
#include "generate.h"
#include "string_tools.h"
#include <chrono>
#include <fstream>
#include <future>
#include <streambuf>
#include <string>
#include <vector>

namespace fplus
{

// API search type: sleep_for_n_seconds : int -> io ()
inline
std::function<void()> sleep_for_n_seconds(std::size_t seconds)
{
    return [seconds]()
    {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    };
}

// API search type: sleep_for_n_milliseconds : int -> io ()
inline
std::function<void()> sleep_for_n_milliseconds(std::size_t milliseconds)
{
    return [milliseconds]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    };
}

// API search type: sleep_for_n_microseconds : int -> io ()
inline
std::function<void()> sleep_for_n_microseconds(std::size_t microseconds)
{
    return [microseconds]()
    {
        std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
    };
}

// API search type: execute_serially : [io ()] -> io ()
template <typename Container,
        typename Effect = typename Container::value_type,
        typename Result = typename utils::function_traits<Effect>::result_type>
std::function<std::vector<Result>()> execute_serially(const Container& effs)
{
    return [effs]() -> std::vector<Result>
    {
        std::vector<Result> results;
        for (const Effect& e : effs)
        {
            results.push_back(e());
        }
        return results;
    };
}

// API search type: execute_serially_until_success : [io bool] -> io bool
template <typename Container,
        typename Effect = typename Container::value_type,
        typename Result = typename utils::function_traits<Effect>::result_type>
std::function<bool()> execute_serially_until_success(const Container& effs)
{
    static_assert(std::is_convertible<Result, bool>::value, "Effects must return a boolish type.");
    return [effs]() -> bool
    {
        for (const Effect& e : effs)
        {
            if (e())
            {
                return true;
            }
        }
        return false;
    };
}

// API search type: execute_and_return_fixed_value : a, [io b] -> io a
template <typename Result, typename Effect>
std::function<Result()> execute_and_return_fixed_value(
        Result result,
        Effect eff)
{
    return [eff, result]() -> Result
    {
        eff();
        return result;
    };
}

// API search type: effect_to_std_function : io a -> io a
template <typename Effect,
        typename Result = typename utils::function_traits<Effect>::result_type>
std::function<Result()> effect_to_std_function(Effect eff)
{
    return [eff]() -> Result
    {
        return eff();
    };
}

// API search type: execute_max_n_times_until_success : int, io (), int -> io bool
template <typename Effect,
        typename Result = typename utils::function_traits<Effect>::result_type>
std::function<bool()> execute_max_n_times_until_success(
        std::size_t n,
        const Effect& eff,
        std::size_t pause_in_milliseconds = 0)
{
    if (pause_in_milliseconds > 0)
    {
        auto sleep_and_return_false =
            execute_and_return_fixed_value(
                false,
                sleep_for_n_milliseconds(pause_in_milliseconds));
        return execute_serially_until_success(
            intersperse(
                sleep_and_return_false,
                replicate(n, effect_to_std_function(eff))));
    }
    return execute_serially_until_success(
        replicate(n, effect_to_std_function(eff)));
}

// API search type: execute_serially_until_failure : [io bool] -> io bool
template <typename Container,
        typename Effect = typename Container::value_type,
        typename Result = typename utils::function_traits<Effect>::result_type>
std::function<bool()> execute_serially_until_failure(const Container& effs)
{
    static_assert(std::is_convertible<Result, bool>::value, "Effects must return a boolish type.");
    return [effs]() -> bool
    {
        for (const Effect& e : effs)
        {
            if (!e())
            {
                return false;
            }
        }
        return true;
    };
}

// API search type: execute_parallelly : [io a] -> io [a]
template <typename Container,
        typename Effect = typename Container::value_type,
        typename Result = typename utils::function_traits<Effect>::result_type>
std::function<std::vector<Result>()> execute_parallelly(const Container& effs)
{
    return [effs]() -> std::vector<Result>
    {
        std::vector<std::future<Result>> handles =
            transform([](Effect e) -> std::future<Result>
            {
                return std::async(std::launch::async, e);
            }, effs);

        std::vector<Result> results;
        results.reserve(size_of_cont(handles));
        for (std::future<Result>& handle : handles)
        {
            results.push_back(handle.get());
        }
        return results;
    };
}

// API search type: execute_fire_and_forget : io a -> io a
template <typename Effect>
std::function<void()> execute_fire_and_forget(Effect eff)
{
    return [eff]()
    {
        std::thread t(eff);
        t.detach();
    };
}

// API search type: read_text_file : string -> io string
inline
std::string read_text_file(const std::string& filename)
{
    std::ifstream input(filename);
    return std::string(
            std::istreambuf_iterator<std::string::value_type>(input),
            std::istreambuf_iterator<std::string::value_type>());
}

// API search type: read_text_file_lines : string -> io [string]
inline
std::vector<std::string> read_text_file_lines(const std::string& filename)
{
    return split_lines(read_text_file(filename), true);
}

// API search type: write_text_file : string, string -> io bool
inline
bool write_text_file(const std::string& filename, const std::string& content)
{
    std::ofstream output(filename);
    output << content;
    return output.good();
}

// API search type: write_text_file_lines : string, [string], bool -> io bool
inline
bool write_text_file_lines(const std::string& filename,
        const std::vector<std::string>& lines, bool trailing_newline = true)
{
    std::string content = join(std::string("\n"), lines);
    if (trailing_newline)
    {
        content += "\n";
    }
    return write_text_file(filename, content);
}

} // namespace fplus