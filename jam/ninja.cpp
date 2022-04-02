#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "lists.h"
#include "parse.h"
#include "rules.h"

using namespace std;


void output_rule(RULE *rule)
{
	std::vector<std::string> lines;

	std::string ruleName = rule->name;
	std::string actions = rule->actions;

	std::string::size_type start = 0;
	std::string::size_type newline = actions.find('\n');

	while (newline != std::string::npos)
	{
		lines.push_back(actions.substr(start, newline - start));
		start = newline + 1;
		newline = actions.find('\n', start + 1);
	}

	std::cout << "rule " << ruleName << std::endl;
	std::cout << "  command = ";
	for (auto index = 0; index < lines.size(); ++index)
	{
		auto line = lines[index];

		bool isEmpty = true;
		// need to translate into ninja style command
		for (std::string::size_type pos = 0; pos < line.size(); ++pos)
		{
			if (isEmpty && (line[pos] == ' ' || line[pos] == '\t'))
			{
				continue;
			}

			isEmpty = false;

			switch (line[pos])
			{
				case '$':
					if (line[pos+1] == '(')
					{
						auto end = line.find(')', pos+1);
						auto var = line.substr(pos+2, end - (pos + 2));
						if (var == "<")
						{
							std::cout << "$out";
						}
						else if (var == ">")
						{
							std::cout << "$in";
						}
						else
						{
							std::cout << "${" << var << "}";
						}
						
						pos = end;
						break;
					}
					else
					{
						std::cout << "$$";
					}
					break;
				default:
					char ch = line[pos];
					std::cout << ch;
					break;
			}
		}

		if (!isEmpty && index != lines.size() - 1) {
			std::cout << " $\n    ";
		}
	}

	std::cout << std::endl << std::endl;
}
