/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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