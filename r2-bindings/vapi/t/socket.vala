/* valac --profile=posix --pkg r_socket socket.vala */

using Posix;
using GLib;
using Radare;

public static void main(string[] args)
{
	string str = (string) new char[4096];
	//unowned GLib.FileStream stdin = GLib.stdin;
	//unowned GLib.FileStream stdout = GLib.stdout;

	RSocket? fd = null;
	if (args.length>2)
		fd = RSocket.connect(args[1], args[2].to_int(), 0);
	else fd = RSocket.connect("radare.org", 80, 0);

	//var fd = RSocket.connect("localhost", 9999);
	if (fd == null) {
		printf("Cannot connect\n");
		return;
	}
	print ("Connected\n");

	print ("[-] waiting for output\n");
///	while (!fd.ready(0,0));

	print ("[-] reading data\n");
	fd.printf ("GET /\r\n\r\n");
/*
	do {
		string s = (string) new char[1024];
		//stdin.scanf("%s", s);
		//stdout.printf("==> (%s)\n", s);
		print ("length is = %d\n", (int)s.size());
		fd.printf ("GET %s HTTP/1.1\r\nHost: radare.org\r\n\r\n", s);
		if (fd.gets (str, 1024)>0)
			printf (str+"\n");
		else break;
	} while (true);
*/
	
	while (fd.gets (ref str, 1024)>0) {
		printf (str+"\n");
	}
	fd.close();
}
