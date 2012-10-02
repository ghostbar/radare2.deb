/* radare - LGPL - Copyright 2009 nibble<.ds@gmail.com> */

[CCode (cheader_filename="r_lang.h", cprefix="r_lang_", lower_case_cprefix="r_lang_")]
namespace Radare {
	[Compact]
	[CCode (cname="RLang", free_function="r_lang_free", cprefix="r_lang_")]
	public class RLang {
		public RLang ();
		public bool define(string type, string name, void* ptr);
		public bool @add(RLang.Plugin plugin);
		public bool use(string name);
		public void undef(string name);
		public bool list();
		public bool set_argv(int argc, char **argv);
		public bool run(string code, int len);
		public bool run_file(string file);
		public bool prompt();

		[Compact]
		[CCode (cname="RLangPlugin", destroy_function="", free_function="" )]
		public class Plugin {
			public string name;
			public string desc;
			//public string *help;
			// TODO: Add missing delegates
		}

		public Plugin cur;
	}
}

