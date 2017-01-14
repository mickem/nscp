/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
std::string valueToQuotedString( const char *value )
{
   // Not sure how to handle unicode...
   if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL && !containsControlCharacter( value ))
	  return std::string("\"") + value + "\"";
   // We have to walk value and escape any special characters.
   // Appending to std::string is not efficient, but this should be rare.
   // (Note: forward slashes are *not* rare, but I am not escaping them.)
   unsigned maxsize = strlen(value)*2 + 3; // allescaped+quotes+NULL
   std::string result;
   result.reserve(maxsize); // to avoid lots of mallocs
   result += "\"";
   for (const char* c=value; *c != 0; ++c)
   {
	  switch(*c)
	  {
		 case '\"':
			result += "\\\"";
			break;
		 case '\\':
			result += "\\\\";
			break;
		 case '\b':
			result += "\\b";
			break;
		 case '\f':
			result += "\\f";
			break;
		 case '\n':
			result += "\\n";
			break;
		 case '\r':
			result += "\\r";
			break;
		 case '\t':
			result += "\\t";
			break;
		 //case '/':
			// Even though \/ is considered a legal escape in JSON, a bare
			// slash is also legal, so I see no reason to escape it.
			// (I hope I am not misunderstanding something.
			// blep notes: actually escaping \/ may be useful in javascript to avoid </
			// sequence.
			// Should add a flag to allow this compatibility mode and prevent this
			// sequence from occurring.
		 default:
			if ( isControlCharacter( *c ) )
			{
			   std::ostringstream oss;
			   oss << "\\u" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << static_cast<int>(*c);
			   result += oss.str();
			}
			else
			{
			   result += *c;
			}
			break;
	  }
   }
   result += "\"";
   return result;
}
*/