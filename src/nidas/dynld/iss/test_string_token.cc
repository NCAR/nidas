/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "string_token.h"
#include <iostream>
#include <iterator>

struct test_case
{
  const char* question;
  const char* answers[10];
};

test_case test_cases[] = {
  { "a,b,c,d,", { "a", "b", "c", "d", "", 0 } },
  { ",123,123,,123,", { "", "123", "123", "", "123", "", 0 } },
  { ",", { "", "", 0 } },
  { "hello,good-bye,hello-again", { "hello", "good-bye", "hello-again", 0 } }
};



int
main(int argc, const char* argv[])
{
  int nerrors = 0;
  for (int i = 1; i < argc; ++i)
  {
    std::vector<std::string> results;
    std::cout << argv[i] << ":\n";
    string_token(results, argv[i]);
    std::ostream_iterator<std::string> out(std::cout, "\n");
    std::copy(results.begin(), results.end(), out);
  }    

  int ntests = sizeof(test_cases)/sizeof(test_cases[0]);
  for (int i = 0; i < ntests; ++i)
  {
    std::vector<std::string> results;
    std::cout << test_cases[i].question << ": ";
    string_token(results, test_cases[i].question);
    std::ostream_iterator<std::string> out(std::cout, " ");
    std::copy(results.begin(), results.end(), out);
    std::cout << "\n";
    std::vector<std::string> answers;
    const char** ap = test_cases[i].answers;
    while (*ap != 0)
    {
      answers.push_back(*ap);
      ++ap;
    }
    if (answers != results)
    {
      std::cerr << "*** test failed. ***\n";
      ++nerrors;
    }
  }    
}



