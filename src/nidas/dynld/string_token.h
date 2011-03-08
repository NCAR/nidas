// -*- mode: C++; c-basic-offset: 4; -*-

#include <string>
#include <vector>

std::string &
trim (std::string &s)
{
    std::string::size_type i = 0;
    while (i < s.length() && isspace (s[i]))
	++i;
    s.erase (0, i);
    i = s.length() - 1;
    while (i > 0 && isspace (s[i]))
	--i;
    s.erase (i + 1, std::string::npos);
    return s;
}


/**
 * Split a string up into a vector of substrings delimited by commas.
 **/
void
string_token(std::vector<std::string>& substrings, const std::string& text, 
	     const std::string& delims = ",")
{
    std::string::size_type len = text.length();
    std::string::size_type pos = 0;
    std::string::size_type next;
    do
    {
	next = std::string::npos;
	if (pos < len)
	    next = text.find_first_of(delims, pos);
	std::string::size_type sublen = 
	    (next == std::string::npos) ? len - pos : next - pos;
	std::string ss = text.substr(pos, sublen);
	substrings.push_back(trim(ss));
	pos += sublen + 1;
    }
    while (next != std::string::npos);
}


