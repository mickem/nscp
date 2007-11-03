// XAutoBuild.cs  Version 1.0
//
// Author:  Hans Dietrich
//          hdietrich@gmail.com
//
// Description:
//     The XAutoBuild utility automatically increments the build number
//     in the file AutoBuild.h.
//
// History
//     Version 1.0 - 2007 June 6
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

//#define XAUTOBUILD_VERBOSE

using System;
using System.IO;
using XGetoptCS;

namespace XAutoBuild
{
	class Version
	{
		public Version(bool verbose)
		{
			_filever    = new uint [4] { 1, 0, 0, 1 };
			_productver = new uint [4] { 1, 0, 0, 1 };
			_autoIncrement = true;
			_verbose = verbose;
		}

		/// <summary>
		/// ReadVersion() reads the information from AutoBuild.h
		/// </summary>
		/// <param name="path">fully qualified path to AutoBuild.h</param>
		/// <returns>true = success</returns>
		public bool ReadVersion(string path)
		{
			_filever[0] = 1;
			_filever[1] = 0;
			_filever[2] = 0;
			_filever[3] = 1;
			_productver[0] = 1;
			_productver[1] = 0;
			_productver[2] = 0;
			_productver[3] = 1;
			_autoIncrement = true;

			StreamReader xabfile = null;

			try
			{
				xabfile = new StreamReader(path);
			}
			catch
			{
				Console.WriteLine("XAutoBuild: Error opening XAutoBuild file {0}.", path);
				return false;
			}

			string line;
			int pos;
			while ((line = xabfile.ReadLine()) != null)
			{
				line = line.ToUpper();

				if ((pos = line.IndexOf("INCREMENT_VERSION")) >= 0)
				{
					pos += "INCREMENT_VERSION".Length + 1;
					string flagIncrement = line.Substring(pos);
					if (flagIncrement.IndexOf("FALSE") >= 0)
						_autoIncrement = false;
					if (_verbose)
						Console.WriteLine("XAutoBuild: _autoIncrement = {0}", _autoIncrement);
					continue;
				}

				if ((pos = line.IndexOf("FILEVER")) >= 0)
				{
					pos += "FILEVER".Length + 1;
					string temp = line.Substring(pos).Trim();
					string [] filever = temp.Split(",".ToCharArray());
					int i = 0;
					foreach (string s in filever)
					{
						if (s.Length > 0)
							_filever[i] = Convert.ToUInt32(s);
						if (++i > 3)
							break;
					}
					if (_verbose)
						Console.WriteLine("XAutoBuild: _filever = {0}.{1}.{2}.{3}", 
							_filever[0], _filever[1], _filever[2], _filever[3]);
					continue;
				}

				if ((pos = line.IndexOf("PRODUCTVER")) >= 0)
				{
					pos += "PRODUCTVER".Length + 1;
					string temp = line.Substring(pos).Trim();
					string [] productver = temp.Split(",".ToCharArray());
					int i = 0;
					foreach (string s in productver)
					{
						if (s.Length > 0)
							_productver[i] = Convert.ToUInt32(s);
						if (++i > 3)
							break;
					}
					if (_verbose)
						Console.WriteLine("XAutoBuild: _productver = {0}.{1}.{2}.{3}", 
							_productver[0], _productver[1], _productver[2], _productver[3]);
					break;
				}
			}

			xabfile.Close();

			return true;
		}

		/// <summary>
		/// WriteVersion() writes AutoBuild.h based on information from
		/// this class.
		/// </summary>
		/// <param name="path">fully qualified path to AutoBuild.h</param>
		/// <returns>true = success</returns>
		public bool WriteVersion(string path)
		{

			StreamWriter xabfile = null;

			try
			{
				xabfile = new StreamWriter(path);
			}
			catch
			{
				Console.WriteLine("XAutoBuild: Error opening XAutoBuild file {0}.", path);
				return false;
			}

			string auto = "FALSE";
			if (_autoIncrement)
				auto = "TRUE";

			xabfile.WriteLine("#ifndef AUTOBUILD_H");
			xabfile.WriteLine("#define AUTOBUILD_H");
			xabfile.WriteLine("// change the FALSE to TRUE for autoincrement of build number");
			xabfile.WriteLine("#define INCREMENT_VERSION {0}", auto);
			xabfile.WriteLine("#define FILEVER        {0},{1},{2},{3}", _filever[0], _filever[1], _filever[2], _filever[3]);
			xabfile.WriteLine("#define PRODUCTVER     {0},{1},{2},{3}", _productver[0], _productver[1], _productver[2], _productver[3]);
			xabfile.WriteLine("#define STRFILEVER     \"{0}.{1}.{2}.{3}\"", _filever[0], _filever[1], _filever[2], _filever[3]);
			xabfile.WriteLine("#define STRPRODUCTVER  \"{0}.{1}.{2}.{3}\"", _productver[0], _productver[1], _productver[2], _productver[3]);
            xabfile.WriteLine("#define STRPRODUCTDATE  \"{0}\"", DateTime.Now.ToString("yyyy-MM-dd"));
			xabfile.WriteLine("#endif // AUTOBUILD_H");

			xabfile.Close();

			return true;
		}

		/// <summary>
		/// AutoIncrement (readonly) returns the _autoincrement flag.
		/// </summary>
		public bool AutoIncrement
		{
			get
			{
				return _autoIncrement;
			}
		}

		/// <summary>
		/// Increment() increments the file version and product version build numbers.
		/// </summary>
		public void Increment()
		{
			_filever[3] += 1;
			_productver[3] += 1;
		}

		private uint[] _filever, _productver;
		private bool _autoIncrement;
		private bool _verbose;
	}

	class Program
	{
		private const string FILE_NAME = "AutoBuild.h";

		static void Usage()
		{
			Console.WriteLine("XAutoBuild: Copyright (c) 2007 by Hans Dietrich");
			Console.WriteLine("XAutoBuild: Usage: XAutoBuild -f <path to autobuild header> [-v]");
		}

		static int Main(string[] args)
		{
#if XAUTOBUILD_VERBOSE
			int i = 0;
			foreach (string arg in args)
			{
				Console.WriteLine("arg[{0}]: {1}", i, arg);
				i++;
			}
#endif

			string path = string.Empty;
			bool verbose = false;

			char c;
			XGetopt go = new XGetopt();
			while ((c = go.Getopt(args.Length, args, "f:v")) != '\0')
			{
#if XAUTOBUILD_VERBOSE
				Console.WriteLine("Getopt returned '{0}'", c);
#endif

				switch (c)
				{
					case 'f':
						path = go.Optarg;
						break;

					case 'v':
						verbose = true;
						break;

					case '?':
						Console.WriteLine("illegal option or missing arg");
						Usage();
						return 1;
				}
			}

			if (path.Length < 4)
			{
				Usage();
				return 1;
			}

            if (path.EndsWith("\"")) {
                path = path.Substring(0, path.Length - 1);
            }
            path = path + Path.DirectorySeparatorChar;
            try
            {
                path = Path.GetDirectoryName(path);
            }
            catch (System.ArgumentException e)
            {
                Console.WriteLine("Path not valid {0}", path);
                Usage();
                return 1;
            }


			string infile = path + Path.DirectorySeparatorChar + FILE_NAME;

			if (verbose)
				Console.WriteLine("XAutoBuild: XAutoBuild file is {0}", infile);

			Version ver = new Version(verbose);

			if (File.Exists(infile))
			{
				if (verbose)
					Console.WriteLine("XAutoBuild: {0} already exists.", infile);

				string bakfile = infile + ".bak";

				// make backup copy
				try
				{
					if (verbose)
						Console.WriteLine("XAutoBuild: creating {0}", bakfile);
					File.Copy(infile, bakfile, true);
				}
				catch
				{
					Console.WriteLine("XAutoBuild: failed to create {0}", bakfile);
					return 1;
				}
			}
			else
			{
				// file doesn't exist, start with version 1.0.0.1

				if (verbose)
					Console.WriteLine("XAutoBuild: creating {0}", infile);

				if (!ver.WriteVersion(infile))
				{
					return 1;
				}
			}

			// at this point input file does exist - now read values

			if (!ver.ReadVersion(infile))
			{
				return 1;
			}

			if (ver.AutoIncrement)
			{
				if (verbose)
					Console.WriteLine("XAutoBuild: updating {0}", infile);

				ver.Increment();

				if (!ver.WriteVersion(infile))
				{
					return 1;
				}
			}
			else
			{
				if (verbose)
					Console.WriteLine("XAutoBuild: INCREMENT_VERSION set to FALSE, skipping update");
			}

			return 0;
		}
	}
}
