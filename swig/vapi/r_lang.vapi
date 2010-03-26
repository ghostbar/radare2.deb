/* radare - LGPL - Copyright 2009 nibble<.ds@gmail.com> */

[CCode (cheader_filename="r_lang.h", cprefix="r_lang_", lower_case_cprefix="r_lang_")]
namespace Radare {
	[Compact]
	[CCode (cname="struct r_lang_t", free_function="r_lang_free", cprefix="r_lang_")]
	public class RLanguage {
		public RLanguage();
		public bool define(string type, string name, void* ptr);
		public bool @add(RLanguage.Handler handler);
		public bool use(string name);
		public bool undef();
		public bool list();
		public bool set_argv(int argc, char **argv);
		public bool run(string code, int len);
		public bool run_file(string file);
		public bool prompt();

		[Compact]
		[CCode (cname="struct r_lang_handle_t", destroy_function="", free_function="" )]
		public class Handler {
			public string name;
			public string desc;
			public string help;
		}

		public Handler cur;
	}
}

