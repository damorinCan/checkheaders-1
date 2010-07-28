/*
 * checkheaders - check headers in C/C++ code
 * Copyright (C) 2010 Daniel Marjam�ki.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#include "checkheaders.h"
#include "tokenize.h"
#include "commoncheck.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <cstring>
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// HEADERS - No implementation in a header
//---------------------------------------------------------------------------

void WarningHeaderWithImplementation(const Tokenizer &tokenizer, bool XmlOutput, std::ostream &errout)
{
    for (const Token *tok = tokenizer.tokens; tok; tok = tok->next)
    {
        // Only interested in included file
        if (tok->FileIndex == 0)
            continue;

        if (Match(tok, ") const| {"))
        {
            std::ostringstream ostr;
            ostr << "Found implementation in header";
            ReportErr(tokenizer, XmlOutput, tok, "HeaderWithImplementation", ostr.str(), errout);
        }
    }
}
//---------------------------------------------------------------------------








//---------------------------------------------------------------------------
// HEADERS - Unneeded include
//---------------------------------------------------------------------------

void WarningIncludeHeader(const Tokenizer &tokenizer, bool XmlOutput, std::ostream &errout)
{
    // Including..
    for (const Token *includetok = tokenizer.tokens; includetok; includetok = includetok->next)
    {
        if (strcmp(includetok->str, "#include") != 0)
            continue;

        // Get fileindex of included file..
        unsigned int hfile = 0;
        const char *includefile = includetok->next->str;
        while (hfile < tokenizer.ShortFileNames.size())
        {
            if ( SameFileName( tokenizer.ShortFileNames[hfile].c_str(), includefile ) )
                break;
            hfile++;
        }
        if (hfile == tokenizer.ShortFileNames.size())
            continue;

        // This header is needed if:
        // * It contains some needed class declaration
        // * It contains some needed function declaration
        // * It contains some needed constant value
        // * It contains some needed variable
        // * It contains some needed enum

        std::set<std::string> classlist;
        std::set<std::string> namelist;

        // Extract classes and names in the header..
        int indentlevel = 0;
        for (const Token *tok1 = tokenizer.tokens; tok1; tok1 = tok1->next )
        {
            if ( tok1->FileIndex != hfile )
                continue;

            // I'm only interested in stuff that is declared at indentlevel 0
            if (tok1->str[0] == '{')
                indentlevel++;

            else if (tok1->str[0] == '}')
                indentlevel--;

            if (indentlevel != 0)
                continue;

            // Class or namespace declaration..
            // --------------------------------------
            if (Match(tok1,"class %var% {") || Match(tok1,"class %var% :") || Match(tok1,"namespace %var% {") || Match(tok1,"struct %var% {"))
                classlist.insert(getstr(tok1, 1));

            // Variable declaration..
            // --------------------------------------
            else if (Match(tok1, "%type% %var% ;") || Match(tok1, "%type% %var% ["))
                namelist.insert(getstr(tok1, 1));

            else if (Match(tok1, "%type% * %var% ;") || Match(tok1, "%type% * %var% ["))
                namelist.insert(getstr(tok1, 2));

            else if (Match(tok1, "const %type% %var% =") || Match(tok1, "const %type% %var% ["))
                namelist.insert(getstr(tok1, 2));

            else if (Match(tok1, "const %type% * %var% =") || Match(tok1, "const %type% * %var% ["))
                namelist.insert(getstr(tok1, 3));

            // enum..
            // --------------------------------------
            else if (strcmp(tok1->str, "enum") == 0)
            {
                tok1 = tok1->next;
                while (tok1->next && tok1->str[0]!=';')
                {
                    if ( IsName(tok1->str) )
                        namelist.insert(tok1->str);
                    tok1 = tok1->next;
                }
            }
                
            // function..  
            // --------------------------------------
            else if (Match(tok1,"%type% %var% ("))
                namelist.insert(getstr(tok1, 1));

            else if (Match(tok1,"%type% * %var% ("))
                namelist.insert(getstr(tok1, 2));

            else if (Match(tok1,"const %type% %var% ("))
                namelist.insert(getstr(tok1, 2));

            else if (Match(tok1,"const %type% * %var% ("))
                namelist.insert(getstr(tok1, 3));

            // typedef..
            // --------------------------------------
            else if (strcmp(tok1->str,"typedef")==0)
            {
                if (strcmp(getstr(tok1,1),"enum")==0)
                    continue;
                int parlevel = 0;
                while (tok1->next)
                {
                    if ( strchr("({", tok1->str[0]) )
                        parlevel++;

                    else if ( strchr(")}", tok1->str[0]) )
                        parlevel--;

                    else if (parlevel == 0)
                    {
                        if ( tok1->str[0] == ';' )
                            break;

                        if ( Match(tok1, "%var% ;") )
                            namelist.insert(tok1->str);
                    }

                    tok1 = tok1->next;
                }
            }

            // #define..
            // --------------------------------------
            else if (Match(tok1, "#define %var%"))
                namelist.insert(tok1->next->str);
        }


        // Check if the extracted names are used...
        bool Needed = false;
        bool NeedDeclaration = false;
        indentlevel = 0;
        for (const Token *tok1 = tokenizer.tokens; tok1; tok1 = tok1->next)
        {
            if (tok1->FileIndex != includetok->FileIndex)
                continue;

            // implementation begins..
            if (indentlevel == 0 && (Match(tok1, ") {") || Match(tok1, ") const {")))
            {
                // Go to the "{"
                while (tok1->str[0] != '{')
                    tok1 = tok1->next;
                indentlevel = 1;
			}
            else if (indentlevel >= 1)
            {
                if (tok1->str[0] == '{')
                    ++indentlevel;
                else if (tok1->str[0] == '}')
                    --indentlevel;
            }

            if ( Match(tok1, ": %var% {") || Match(tok1, ": %type% %var% {") )
            {
                const std::string classname(getstr(tok1, (strcmp(getstr(tok1,2),"{")) ? 2 : 1));
                if (classlist.find(classname) != classlist.end())
                {
                    Needed = true;
                    break;
                }
            }

            if (indentlevel == 0 && Match(tok1, "* %var%"))
            {
                if (classlist.find(tok1->str) != classlist.end())
                {
                    NeedDeclaration = true;
                    tok1 = tok1->next;
                    continue;
                }
            }

            if ( ! IsName(tok1->str) )
                continue;

            if (namelist.find(tok1->str) != namelist.end() ||
                classlist.find(tok1->str) != classlist.end())
            {
                Needed = true;
                break;
            }
        }

        // Not needed!
        if (!Needed)
        {
            std::ostringstream ostr;
            ostr << "The included header '" << includefile << "' is not needed";
            if (NeedDeclaration)
                ostr << " (but a forward declaration is needed)";
            ReportErr(tokenizer, XmlOutput, includetok, "HeaderNotNeeded", ostr.str(), errout);
        }
    }
}
//---------------------------------------------------------------------------



