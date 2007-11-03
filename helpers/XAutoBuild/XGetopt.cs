// XGetopt.cs  Version 1.0
//
// Author:  Hans Dietrich
//        hdietrich@gmail.com
//
// Description:
//     The Getopt() method parses command line arguments. It is modeled
//     after the Unix function getopt().  Its parameters argc and argv are
//     the argument count and array as passed into the application on program 
//     invocation. Getopt returns the next option letter in argv that 
//     matches a letter in optstring.
//
//     optstring is a string of allowable option letters;  if a letter is 
//     followed by a colon, the option is expected to have an argument that
//     may or may not be separated from it by white space.  optarg contains
//     the option argument on return from Getopt (use the Optarg property).
//
//     Option letters may be combined, e.g., "-ab" is equivalent to "-a -b".  
//     Option letters are case sensitive.
//
//     Getopt places in the internal variable optind the argv index of the
//     next argument to be processed.  optind is initialized to 0 before the 
//     first call to Getopt.  Use the Optind property to query the optind 
//     value.
//
//     When all options have been processed (i.e., up to the first non-option
//     argument), Getopt returns '\0', optarg will contain the argument, 
//     and optind will be set to the argv index of the argument.  If there 
//     are no non-option arguments, optarg will be Empty.
//
//     The special option "--" may be used to delimit the end of the options;
//     '\0' will be returned, and "--" (and everything after it) will be 
//     skipped.
//
// Return Value:
//     For option letters contained in the string optstring, Getopt will
//     return the option letter.  Getopt returns a question mark ('?') when
//     it encounters an option letter not included in optstring. '\0' is 
//     returned when processing is finished.
//
// Limitations:
//     1)  Long options are not supported.
//     2)  The GNU double-colon extension is not supported.
//     3)  The environment variable POSIXLY_CORRECT is not supported.
//     4)  The + syntax is not supported.
//     5)  The automatic permutation of arguments is not supported.
//     6)  This implementation of Getopt() returns '\0' if an error is
//         encountered, instead of -1 as the latest standard requires.
//     7)  This implementation of Getopt() returns a char instead of an int.
//
// Example:
//     static int Main(string[] args)
//     {
//         int argc = args.Length;
//         char c;
//         XGetopt go = new XGetopt();
//         while ((c = go.Getopt(argc, args, "aBn:")) != '\0')
//         {
//             switch (c)
//             {
//                 case 'a':
//                     Console.WriteLine("option -a");
//                     break;
//
//                 case 'B':
//                     Console.WriteLine("option -B");
//                     break;
//
//                 case 'n': 
//                     Console.WriteLine("option -n with arg '{0}'", go.Optarg);
//                     break;
//
//                 case '?':
//                     Console.WriteLine("illegal option or missing arg");
//                     return 1;
//             }
//         }
//
//         if (go.Optarg != string.Empty)
//             Console.WriteLine("non-option arg '{0}'", go.Optarg);
//
//         ...
//
//         return 0;
//     }
//
// History:
//     Version 1.0 - 2007 June 5
//     - Initial public release
//
// License:
//     This software is released into the public domain.  You are free to use
//     it in any way you like, except that you may not sell this source code.
//
//     This software is provided "as is" with no expressed or implied warranty.
//     I accept no liability for any damage or loss of business that this 
//     software may cause.
//
///////////////////////////////////////////////////////////////////////////////

//#define XGETOPT_VERBOSE

using System;
using System.Collections.Generic;
using System.Text;

namespace XGetoptCS
{
	public class XGetopt
	{
		#region Class data

		private int optind;
		private string nextarg;
		private string optarg;
		
		#endregion

		#region Class properties

		public string Optarg
		{
			get
			{
				return optarg;
			}
		}

		public int Optind
		{
			get
			{
				return optind;
			}
		}

		#endregion

		#region Class public methods

		public XGetopt()
		{
			Init();
		}

		public void Init()
		{
			optind = 0;
			optarg = string.Empty;
			nextarg = string.Empty;
		}

		public char Getopt(int argc, string[] argv, string optstring)
		{
#if XGETOPT_VERBOSE
			Console.WriteLine("Getopt: argc = {0}", argc);
#endif

			optarg = string.Empty;

			if (argc < 0)
				return '?';

#if XGETOPT_VERBOSE
			if (optind < argc)
				Console.WriteLine("Getopt: argv[{0}] = {1}", optind, argv[optind]);
#endif

			if (optind == 0)
				nextarg = string.Empty;

			if (nextarg.Length == 0)
			{
				if (optind >= argc || argv[optind][0] != '-' || argv[optind].Length < 2)
				{
					// no more options
					optarg = string.Empty;
					if (optind < argc)
						optarg = argv[optind];	// return leftover arg
					return '\0';
				}

				if (argv[optind] == "--")
				{
					// 'end of options' flag
					optind++;
					optarg = string.Empty;
					if (optind < argc)
						optarg = argv[optind];
					return '\0';
				}

				nextarg = string.Empty;
				if (optind < argc)
				{
					nextarg = argv[optind];
					nextarg = nextarg.Substring(1);		// skip past -
				}
				optind++;
			}

			char c = nextarg[0];				// get option char
			nextarg = nextarg.Substring(1);		// skip past option char
			int index = optstring.IndexOf(c);	// check if this is valid option char

			if (index == -1 || c == ':')
				return '?';

			index++;
			if ((index < optstring.Length) && (optstring[index] == ':'))
			{
				// option takes an arg
				if (nextarg.Length > 0)
				{
					optarg = nextarg;
					nextarg = string.Empty;
				}
				else if (optind < argc)
				{
					optarg = argv[optind];
					optind++;
				}
				else
				{
					return '?';
				}
			}

			return c;
		}

		#endregion
	}
}
