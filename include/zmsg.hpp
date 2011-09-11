/*  =========================================================================
    zmsg.hpp

    Multipart message class for example applications.

    Follows the ZFL class conventions and is further developed as the ZFL
    zfl_msg class.  See http://zfl.zeromq.org for more details.

    -------------------------------------------------------------------------
    Copyright (c) 1991-2010 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of the ZeroMQ Guide: http://zguide.zeromq.org

    This is free software; you can redistribute it and/or modify it under the
    terms of the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your option)
    any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABIL-
    ITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
    Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================

    Andreas Hoelzlwimmer <andreas.hoelzlwimmer@fh-hagenberg.at>
*/

#ifndef __ZMSG_H_INCLUDED__
#define __ZMSG_H_INCLUDED__

//#include "zhelpers.hpp"

#include <vector>
#include <string>
#include <boost/optional.hpp>
#include <boost/type.hpp>

class zmsg {
public:
    //typedef std::basic_string<unsigned char> ustring;

	typedef boost::optional<std::string> opstring_t;

    zmsg() {
    }

   //  --------------------------------------------------------------------------
   //  Constructor, sets initial body
	zmsg(std::string body) {
       body_set(body);
   }

   //  -------------------------------------------------------------------------
   //  Constructor, sets initial body and sends message to socket
   zmsg(std::string body, zmq::socket_t &socket) {
       body_set(body);
       send(socket);
   }

   //  --------------------------------------------------------------------------
   //  Constructor, calls first receive automatically
   zmsg(zmq::socket_t &socket) {
	   if (!recv(socket)) {
		   std::wcout << _T("-------------------- DAMN -------------------") << std::endl;
	   }
   }

   //  --------------------------------------------------------------------------
   //  Copy Constructor, equivalent to zmsg_dup
   zmsg(zmsg &msg) {
       m_part_data.resize(msg.m_part_data.size());
       std::copy(msg.m_part_data.begin(), msg.m_part_data.end(), m_part_data.begin());
   }

   virtual ~zmsg() {
      clear();
   }

   //  --------------------------------------------------------------------------
   //  Erases all messages
   void clear() {
       m_part_data.clear();
   }

   void set_part(size_t part_nbr, std::string data) {
       if (part_nbr < m_part_data.size() && part_nbr >= 0) {
           m_part_data[part_nbr] = data;
       }
   }

   inline std::string msg_to_string(zmq::message_t &message) {
	   return std::string(reinterpret_cast<char*>(message.data()), message.size());
   }
   bool recv(zmq::socket_t & socket) {
      clear();
      while(1) {
         zmq::message_t message(0);
         try {
            if (!socket.recv(&message, 0)) {
               return false;
            }
         } catch (zmq::error_t error) {
            //std::cout << "E: " << error.what() << std::endl;
            return false;
         }
         char *data = reinterpret_cast<char*>(message.data());
         //std::cerr << "recv: \"" << (unsigned char*) message.data() << "\", size " << message.size() << std::endl;
         if (message.size() == 17 && data[0] == 0) {
            push_back(encode_uuid(msg_to_string(message)));
         } else {
            push_back(msg_to_string(message));
         }
		 boost::int64_t more;
         size_t more_size = sizeof(more);
         socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
         if (!more) {
            break;
         }
      }
      return true;
   }

   void send(zmq::socket_t & socket) {
       for (size_t part_nbr = 0; part_nbr < m_part_data.size(); part_nbr++) {
          zmq::message_t message;
		  std::string data = m_part_data[part_nbr];
          if (data.size() == 33 && data [0] == '@') {
             unsigned char * uuidbin = decode_uuid ((char *) data.c_str());
             message.rebuild(17);
             memcpy(message.data(), uuidbin, 17);
             delete uuidbin;
          } else {
             message.rebuild(data.size());
             memcpy(message.data(), data.c_str(), data.size());
          }
          try {
			  //dump();
             socket.send(message, part_nbr < m_part_data.size() - 1 ? ZMQ_SNDMORE : 0);
          } catch (zmq::error_t error) {
             //assert(error.num()!=0);
          }
       }
       clear();
   }

   size_t parts() {
      return m_part_data.size();
   }

   void body_set(const std::string body) {
      if (m_part_data.size() > 0) {
         m_part_data.erase(m_part_data.end()-1);
      }
      push_back(body);
   }


   boost::optional<std::string> body ()
   {
       if (m_part_data.size())
           return boost::optional<std::string>(m_part_data[m_part_data.size() - 1]);
       else
           return boost::optional<std::string>();
   }

   // zmsg_push
   void push_front(std::string part) {
      m_part_data.insert(m_part_data.begin(), part);
   }

   // zmsg_append
   void push_back(std::string part) {
      m_part_data.push_back(part);
   }

   //  --------------------------------------------------------------------------
   //  Formats 17-byte UUID as 33-char string starting with '@'
   //  Lets us print UUIDs as C strings and use them as addresses
   //
   static std::string encode_uuid(std::string data) {
       static char
           hex_char [] = "0123456789ABCDEF";

       assert(data[0] == 0);
	   std::string uuidstr = std::string(34, ' ');
       uuidstr[0] = '@';
       int byte_nbr;
       for (byte_nbr = 0; byte_nbr < 16; byte_nbr++) {
           uuidstr[byte_nbr * 2 + 1] = hex_char[data[byte_nbr + 1] >> 4];
           uuidstr[byte_nbr * 2 + 2] = hex_char[data[byte_nbr + 1] & 15];
       }
       uuidstr[33] = 0;
       return uuidstr;
   }


   // --------------------------------------------------------------------------
   // Formats 17-byte UUID as 33-char string starting with '@'
   // Lets us print UUIDs as C strings and use them as addresses
   //
   static unsigned char *
   decode_uuid (char *uuidstr)
   {
       static char
           hex_to_bin [128] = {
              -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* */
              -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* */
              -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* */
               0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1, /* 0..9 */
              -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* A..F */
              -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* */
              -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* a..f */
              -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }; /* */

       assert (strlen (uuidstr) == 33);
       assert (uuidstr [0] == '@');
       unsigned char *data = new unsigned char[17];
       int byte_nbr;
       data [0] = 0;
       for (byte_nbr = 0; byte_nbr < 16; byte_nbr++)
           data [byte_nbr + 1]
               = (hex_to_bin [uuidstr [byte_nbr * 2 + 1] & 127] << 4)
               + (hex_to_bin [uuidstr [byte_nbr * 2 + 2] & 127]);

       return (data);
   }

   // zmsg_pop
   boost::optional<std::string> pop_front() {
      if (m_part_data.size() == 0) {
         return boost::optional<std::string>();
      }
	  std::string part = m_part_data.front();
      m_part_data.erase(m_part_data.begin());
      return boost::optional<std::string>(part);
   }

   void append(std::string part) {
       push_back(part);
   }

   boost::optional<std::string> address() {
      if (m_part_data.size()>0) {
         return boost::optional<std::string>(m_part_data[0]);
      } else {
		  return boost::optional<std::string>();
      }
   }

   void wrap(std::string address, boost::optional<std::string> delim) {
      if (delim)
         push_front(*delim);
      push_front(address);
   }
   void wrap(std::string address, std::string delim) {
	   wrap(address, opstring_t(delim));
   }
   void wrap(std::string address) {
	   wrap(address, opstring_t());
   }

   boost::optional<std::string> unwrap() {
      if (m_part_data.size() == 0) {
         return opstring_t();
      }
	  opstring_t addr = pop_front();
      if (address()) {
         pop_front();
      }
      return opstring_t(addr);
   }

   void dump() {
      std::cerr << "--------------------------------------" << std::endl;
      for (unsigned int part_nbr = 0; part_nbr < m_part_data.size(); part_nbr++) {
		  std::string data = m_part_data [part_nbr];

          // Dump the message as text or binary
          int is_text = 1;
          for (unsigned int char_nbr = 0; char_nbr < data.size(); char_nbr++)
              if (data [char_nbr] < 32 || data [char_nbr] > 127)
                  is_text = 0;

          std::cerr << "[" << std::setw(3) << std::setfill('0') << (int) data.size() << "] ";
          for (unsigned int char_nbr = 0; char_nbr < data.size(); char_nbr++) {
              if (is_text) {
                  std::cerr << (char) data [char_nbr];
              } else {
                  std::cerr << std::hex << std::setw(2) << std::setfill('0') << (short int) data [char_nbr];
              }
          }
          std::cerr << std::endl;
      }
	  std::cerr << "--------------------------------------" << std::endl;
   }

   static int
   test(int verbose)
   {
      zmq::context_t context(5);
      zmq::socket_t output(context, ZMQ_XREQ);
      try {
         output.bind("ipc://zmsg_selftest.ipc");
      } catch (zmq::error_t error) {
         //assert(error.num()!=0);
      }
      zmq::socket_t input(context, ZMQ_XREP);
      try {
         input.connect("ipc://zmsg_selftest.ipc");
      } catch (zmq::error_t error) {
         //assert(error.num()!=0);
      }

      zmsg zm;
      zm.body_set((char *)"Hello");
      assert (*zm.body() == "Hello");
	  std::cout << "body is correct" << std::endl;

      zm.send(output);
      assert(zm.parts() == 0);
	  std::cout << "sent message" << std::endl;

      zm.recv(input);
      assert (zm.parts() == 2);
	  std::cout << "received message" << std::endl;
      if(verbose) {
         zm.dump();
      }

      assert (zm.body() && *zm.body() == "Hello");

      zm.clear();
      zm.body_set("Hello");
      zm.wrap("address1", "");
      zm.wrap("address2");
      assert (zm.parts() == 4);
      zm.send(output);

      zm.recv(input);
      if (verbose) {
         zm.dump();
      }
      assert (zm.parts() == 5);
      assert (zm.address()->size() == 33);
      zm.unwrap();
      assert (zm.address() && *zm.address() == "address2");
      //zm.body_fmt ("%c%s", 'W', "orld");
      zm.send(output);

      zm.recv (input);
      zm.unwrap ();
      assert (zm.parts () == 4);
      assert (zm.body() && *zm.body() == "World");
      opstring_t part = zm.unwrap ();
      assert (part && *part == "address2");

      // Pull off address 1, check that empty part was dropped
      part = zm.unwrap ();
      assert (part && *part == "address1");
      assert (zm.parts () == 1);

      // Check that message body was correctly modified
      part = zm.pop_front();
      assert (part && *part == "World");
      assert (zm.parts () == 0);

      // Check append method
      zm.append ("Hello");
      zm.append ("World!");
      assert (zm.parts() == 2);
      assert (zm.body() && *zm.body() == "World!");

      zm.clear();
      assert (zm.parts() == 0);

      std::cout << "OK" << std::endl;
      return 0;
   }

private:
	std::vector<std::string> m_part_data;
};

#endif /* ZMSG_H_ */