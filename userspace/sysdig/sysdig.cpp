/*
Copyright (C) 2013-2014 Draios inc.

This file is part of sysdig.

sysdig is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

sysdig is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sysdig.  If not, see <http://www.gnu.org/licenses/>.
*/

#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>

#include <sinsp.h>
#include "sysdig.h"
#include "chisel.h"

#ifdef _WIN32
#include "win32/getopt.h"
#include <io.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif

static bool g_terminate = false;
#ifdef HAS_CHISELS
vector<sinsp_chisel*> g_chisels;
#endif

static void usage();

//
// Helper functions
//
static void signal_callback(int signal)
{
	g_terminate = true;
}

void replace_in_place(string& str, string substr_to_replace, string new_substr)
{
	size_t index = 0;
	uint32_t nsize = substr_to_replace.size();

	while (true)
	{
		 index = str.find(substr_to_replace, index);
		 if (index == string::npos) break;

		 str.replace(index, nsize, new_substr);

		 index += nsize;
	}
}

//
// Program help
//
static void usage()
{
    printf(
"sysdig version " SYSDIG_VERSION "\n"
"Usage: sysdig [options] [-p <output_format>] [filter]\n\n"
"Options:\n"
" -A, --print-ascii  Only print the text portion of data buffers, and echo\n"
"                    end-of-lines. This is useful to only display human-readable\n"
"                    data.\n"
" -a, --abstime      Show absolute event timestamps\n"
#ifdef HAS_CHISELS
" -c <chiselname> <chiselargs>, --chisel  <chiselname> <chiselargs>\n"
"                    run the specified chisel. If the chisel require arguments,\n"
"                    they must be specified in the command line after the name.\n"
" -cl, --list-chisels\n"
#endif
"                    lists the available chisels. Looks for chisels in .,\n"
"                    ./chisels, ~/.chisels and /usr/share/sysdig/chisels.\n"
" -d, --displayflt   Make the given filter a display one.\n"
"                    Setting this option causes the events to be filtered\n"
"                    after being parsed by the state system. Events are\n"
"                    normally filtered before being analyzed, which is more\n"
"                    efficient, but can cause state (e.g. FD names) to be lost.\n"
" -D, --debug        Capture events about sysdig itself\n"
" -h, --help         Print this page\n"
#ifdef HAS_CHISELS
" -i <chiselname>, --chisel-info <chiselname>\n"
"                    Get a longer description and the arguments associated with\n"
"                    a chisel found in the -cl option list.\n"
#endif
" -j, --json         Emit output as json\n"
" -L, --list-events  List the events that the engine supports\n"
" -l, --list         List the fields that can be used for filtering and output\n"
"                    formatting. Use -lv to get additional information for each\n"
"                    field.\n"
" -n <num>, --numevents=<num>\n"
"                    Stop capturing after <num> events\n"
" -P, --progress     Print progress on stderr while processing trace files\n"
" -p <output_format>, --print=<output_format>\n"
"                    Specify the format to be used when printing the events.\n"
"                    See the examples section below for more info.\n"
" -q, --quiet        Don't print events on the screen.\n"
"                    Useful when dumping to disk.\n"
" -r <readfile>, --read=<readfile>\n"
"                    Read the events from <readfile>.\n"
" -S, --summary      print the event summary (i.e. the list of the top events)\n"
"                    when the capture ends.\n"
" -s <len>, --snaplen=<len>\n"
"                    Capture the first <len> bytes of each I/O buffer.\n"
"                    By default, the first 80 bytes are captured. Use this\n"
"                    option with caution, it can generate huge trace files.\n"
" -t <timetype>, --timetype=<timetype>\n"
"                    Change the way event time is diplayed. Accepted values are\n"
"                    h for human-readable string, a for abosulte timestamp from\n"
"                    epoch, r for relative time from the beginning of the\n"
"                    capture, and d for delta between event enter and exit.\n"
" -v, --verbose      Verbose output.\n"
" -w <writefile>, --write=<writefile>\n"
"                    Write the captured events to <writefile>.\n"
" -x, --print-hex    Print data buffers in hex.\n"
" -X, --print-hex-ascii\n"
"                    Print data buffers in hex and ASCII.\n"
" -z, --compress     Used with -w, enables compression for tracefiles.\n"
"\n"
"Output format:\n\n"
"By default, sysdig prints the information for each captured event on a single\n"
" line with the following format:\n\n"
"<evt.time> %%evt.cpu %%proc.name (%%thread.tid) %%evt.dir %%evt.type %%evt.args\n\n"
"where:\n"
" evt.time is the event timestamp\n"
" evt.cpu is the CPU number where the event was captured\n"
" proc.name is the name of the process that generated the event\n"
" thread.tid id the TID that generated the event, which corresponds to the\n"
"   PID for single thread processes\n"
" evt.dir is the event direction, > for enter events and < for exit events\n"
" evt.type is the name of the event, e.g. 'open' or 'read'\n"
" evt.args is the list of event arguments.\n\n"
"The output format can be customized with the -p switch, using any of the\n"
"fields listed by 'sysdig -l'.\n\n"
"Examples:\n\n"
" Capture all the events from the live system and print them to screen\n"
"   $ sysdig\n\n"
" Capture all the events from the live system and save them to disk\n"
"   $ sysdig -w dumpfile.scap\n\n"
" Read events from a file and print them to screen\n"
"   $ sysdig -r dumpfile.scap\n\n"
" Print all the open system calls invoked by cat\n"
"   $ sysdig proc.name=cat and evt.type=open\n\n"
" Print the name of the files opened by cat\n"
"   $ sysdig -p\"%%evt.arg.name\" proc.name=cat and evt.type=open\n\n"
    );
}

void print_summary_table(sinsp* inspector,
						 vector<summary_table_entry>* summary_table,
						 uint32_t nentries)
{
	sinsp_evttables* einfo = inspector->get_event_info_tables();

	cout << "----------------------\n";
	string tstr = string("Event");
	tstr.resize(16, ' ');
	tstr += "#Calls\n";
	cout << tstr;
	cout << "----------------------\n";

	sort(summary_table->begin(), summary_table->end(),
		summary_table_entry_rsort_comparer());

	for(uint32_t j = 0; j < nentries; j++)
	{
		summary_table_entry* e = &summary_table->at(j);

		if(e->m_ncalls == 0)
		{
			break;
		}

		if(e->m_is_unsupported_syscall)
		{
			tstr = einfo->m_syscall_info_table[e->m_id / 2].name;
			tstr.resize(16, ' ');

			printf("%s%s%" PRIu64 "\n",
				(PPME_IS_ENTER(e->m_id))? "> ": "< ",
				tstr.c_str(),
				e->m_ncalls);
		}
		else
		{
			tstr = einfo->m_event_info[e->m_id].name;
			tstr.resize(16, ' ');

			printf("%s%s%" PRIu64 "\n",
				(PPME_IS_ENTER(e->m_id))? "> ": "< ",
				tstr.c_str(),
				e->m_ncalls);
		}
	}
}

#ifdef HAS_CHISELS
static void add_chisel_dirs(sinsp* inspector)
{
	//
	// Add the default chisel directory statically configured by the build system
	//
	inspector->add_chisel_dir(SYSDIG_INSTALLATION_DIR CHISELS_INSTALLATION_DIR, false);

	//
	// Add the directories configured in the SYSDIG_CHISEL_DIR environment variable
	//
	char* s_user_cdirs = getenv("SYSDIG_CHISEL_DIR");

	if(s_user_cdirs != NULL)
	{
		vector<string> user_cdirs = sinsp_split(s_user_cdirs, ';');

		for(uint32_t j = 0; j < user_cdirs.size(); j++)
		{
			inspector->add_chisel_dir(user_cdirs[j], true);
		}
	}
}
#endif

static void initialize_chisels()
{
#ifdef HAS_CHISELS
	for(uint32_t j = 0; j < g_chisels.size(); j++)
	{
		g_chisels[j]->on_init();
	}
#endif
}

static void free_chisels()
{
#ifdef HAS_CHISELS
	for(vector<sinsp_chisel*>::iterator it = g_chisels.begin();
		it != g_chisels.end(); ++it)
	{
		delete *it;
	}
#endif
}

static void chisels_on_capture_start()
{
#ifdef HAS_CHISELS
	for(uint32_t j = 0; j < g_chisels.size(); j++)
	{
		g_chisels[j]->on_capture_start();
	}
#endif
}

static void chisels_on_capture_end()
{
#ifdef HAS_CHISELS
	for(vector<sinsp_chisel*>::iterator it = g_chisels.begin();
		it != g_chisels.end(); ++it)
	{
		(*it)->on_capture_end();
	}
#endif
}

static void chisels_do_timeout(sinsp_evt* ev)
{
#ifdef HAS_CHISELS
	for(vector<sinsp_chisel*>::iterator it = g_chisels.begin();
		it != g_chisels.end(); ++it)
	{
		(*it)->do_timeout(ev);
	}
#endif
}

//
// Event processing loop
//
captureinfo do_inspect(sinsp* inspector,
					   uint64_t cnt,
					   bool quiet,
					   bool absolute_times,
					   bool print_progress,
					   sinsp_filter* display_filter,
					   vector<summary_table_entry>* summary_table,
					   sinsp_evt_formatter* formatter)
{
	captureinfo retval;
	int32_t res;
	sinsp_evt* ev;
	uint64_t ts;
	uint64_t deltats = 0;
	uint64_t firstts = 0;
	string line;
	double last_printed_progress_pct = 0;

	//
	// Loop through the events
	//
	while(1)
	{
		if(retval.m_nevts == cnt || g_terminate)
		{
			//
			// End of capture, either because the user stopped it, or because
			// we reached the event count specified with -n.
			// Notify the chisels that we're exiting.
			//
			chisels_on_capture_end();
			break;
		}

		res = inspector->next(&ev);

		if(res == SCAP_TIMEOUT)
		{
			if(ev != NULL && ev->is_filtered_out())
			{
				//
				// The event has been dropped by the filtering system.
				// Give the chisels a chance to run their timeout logic.
				//
				chisels_do_timeout(ev);
			}
			continue;
		}
		else if(res == SCAP_EOF)
		{
			//
			// Reached the end of a trace file.
			// If we are reporting prgress, this is 100%
			//
			if(print_progress)
			{
				fprintf(stderr, "100.00\n");
				fflush(stderr);
			}

			//
			// Notify the chisels that we're exiting.
			//
			chisels_on_capture_end();

			break;
		}
		else if(res != SCAP_SUCCESS)
		{
			//
			// Event read error.
			// Notify the chisels that we're exiting, and then die with an error.
			//
			chisels_on_capture_end();
			cerr << "res = " << res << endl;
			throw sinsp_exception(inspector->getlasterr().c_str());
		}

		retval.m_nevts++;

		ts = ev->get_ts();
		if(firstts == 0)
		{
			firstts = ts;
		}
		deltats = ts - firstts;

		if(print_progress)
		{
			if(ev->get_num() % 10000 == 0)
			{
				double progress_pct = inspector->get_read_progress();

				if(progress_pct - last_printed_progress_pct > 0.1)
				{
					fprintf(stderr, "%.2lf\n", progress_pct);
					fflush(stderr);
					last_printed_progress_pct = progress_pct;
				}
			}
		}

		//
		// If there are chisels to run, run them
		//
#ifdef HAS_CHISELS
		if(!g_chisels.empty())
		{
			for(vector<sinsp_chisel*>::iterator it = g_chisels.begin(); it != g_chisels.end(); ++it)
			{
				if((*it)->run(ev) == false)
				{
					continue;
				}
			}
		}
		else
#endif
		{
			//
			// If we're supposed to summarize, increase the count for this event
			//
			if(summary_table != NULL)
			{
				uint16_t etype = ev->get_type();

				if(etype == PPME_GENERIC_E)
				{
					sinsp_evt_param *parinfo = ev->get_param(0);
					uint16_t id = *(int16_t *)parinfo->m_val;
					((*summary_table)[PPM_EVENT_MAX + id * 2]).m_ncalls++;
				}
				else if(etype == PPME_GENERIC_X)
				{
					sinsp_evt_param *parinfo = ev->get_param(0);
					uint16_t id = *(int16_t *)parinfo->m_val;
					((*summary_table)[PPM_EVENT_MAX + id * 2 + 1]).m_ncalls++;
				}
				else
				{
					((*summary_table)[etype]).m_ncalls++;
				}
			}

			//
			// When the quiet flag is specified, we don't do any kind of processing other
			// than counting the events.
			//
			if(quiet)
			{
				continue;
			}

			if(formatter->tostring(ev, &line))
			{
				//
				// Output the line
				//
				if(display_filter)
				{
					if(!display_filter->run(ev))
					{
						continue;
					}
				}

				cout << line << endl;
			}
		}
	}

	retval.m_time = deltats;
	return retval;
}

//
// MAIN
//
int main(int argc, char **argv)
{
	int res = EXIT_SUCCESS;
	sinsp* inspector = NULL;
	vector<string> infiles;
	string outfile;
	int op;
	uint64_t cnt = -1;
	bool quiet = false;
	bool absolute_times = false;
	bool is_filter_display = false;
	bool verbose = false;
	bool list_flds = false;
	bool print_progress = false;
	bool compress = false;
	sinsp_evt::param_fmt event_buffer_format = sinsp_evt::PF_NORMAL;
	sinsp_filter* display_filter = NULL;
	double duration = 1;
	captureinfo cinfo;
	string output_format;
	uint32_t snaplen = 0;
	int long_index = 0;
	int32_t n_filterargs = 0;
	int cflag = 0;
	string cname;
	vector<summary_table_entry>* summary_table = NULL;
	string timefmt = "%evt.time";

	static struct option long_options[] =
	{
		{"print-ascii", no_argument, 0, 'A' },
		{"abstimes", no_argument, 0, 'a' },
#ifdef HAS_CHISELS
		{"chisel", required_argument, 0, 'c' },
		{"list-chisels", no_argument, &cflag, 1 },
#endif
		{"compress", no_argument, 0, 'z' },
		{"displayflt", no_argument, 0, 'd' },
		{"debug", no_argument, 0, 'D'},
		{"help", no_argument, 0, 'h' },
#ifdef HAS_CHISELS
		{"chisel-info", required_argument, 0, 'i' },
#endif
		{"json", no_argument, 0, 'j' },
		{"list", no_argument, 0, 'l' },
		{"list-events", no_argument, 0, 'L' },
		{"numevents", required_argument, 0, 'n' },
		{"progress", required_argument, 0, 'P' },
		{"print", required_argument, 0, 'p' },
		{"quiet", no_argument, 0, 'q' },
		{"readfile", required_argument, 0, 'r' },
		{"snaplen", required_argument, 0, 's' },
		{"summary", no_argument, 0, 'S' },
		{"timetype", required_argument, 0, 't' },
		{"verbose", no_argument, 0, 'v' },
		{"writefile", required_argument, 0, 'w' },
		{"print-hex", no_argument, 0, 'x'},
		{"print-hex-ascii", no_argument, 0, 'X'},
		{0, 0, 0, 0}
	};

	output_format = "*%evt.num <TIME> %evt.cpu %proc.name (%thread.tid) %evt.dir %evt.type %evt.args";
//	output_format = DEFAULT_OUTPUT_STR;

	try
	{
		inspector = new sinsp();

#ifdef HAS_CHISELS
		add_chisel_dirs(inspector);
#endif

		//
		// Parse the args
		//
		while((op = getopt_long(argc, argv, "Aac:dDhi:jlLn:Pp:qr:Ss:t:vw:xXz", long_options, &long_index)) != -1)
		{
			switch(op)
			{
			case 'A':
				if(event_buffer_format != sinsp_evt::PF_NORMAL)
				{
					fprintf(stderr, "you cannot specify more than one output format\n");
					delete inspector;
					return EXIT_SUCCESS;
				}

				event_buffer_format = sinsp_evt::PF_EOLS;
				break;
			case 'a':
				absolute_times = true;
				break;
			case 0:
				if(cflag != 1 && cflag != 2)
				{
					break;
				}

				if(cflag == 2)
				{
					cname = optarg;
				}
#ifdef HAS_CHISELS
			case 'c':
				{
					if(cflag == 0)
					{
						string ostr(optarg);

						if(ostr.size() >= 1)
						{
							if(ostr == "l")
							{
								cflag = 1;
							}
						}
					}

					if(cflag == 1)
					{
						vector<chisel_desc> chlist;
						sinsp_chisel::get_chisel_list(&chlist);
						list_chisels(&chlist);
						delete inspector;
						return EXIT_SUCCESS;
					}

					sinsp_chisel* ch = new sinsp_chisel(inspector, optarg);
					uint32_t nargs = ch->get_n_args();
					vector<string> args;

					for(uint32_t j = 0; j < nargs; j++)
					{
						if(optind + j >= (uint32_t)argc)
						{
							throw sinsp_exception("invalid number of arguments for chisel " + string(optarg) + ", " + to_string((long long int)nargs) + " expected.");
						}

						args.push_back(argv[optind + j]);
						n_filterargs++;
					}

					ch->set_args(&args);

					g_chisels.push_back(ch);
				}
#endif
				break;
			case 'D':
				inspector->set_debug_mode(true);
				break;
#ifdef HAS_CHISELS
			// --chisel-info and -i
			case 'i':
				{
					cname = optarg;

					vector<chisel_desc> chlist;

					sinsp_chisel::get_chisel_list(&chlist);

					for(uint32_t j = 0; j < chlist.size(); j++)
					{
						if(chlist[j].m_name == cname)
						{
							print_chisel_info(&chlist[j]);
							delete inspector;
							return EXIT_SUCCESS;
						}
					}

					throw sinsp_exception("chisel " + cname + " not found - use -cl to list them.");
				}
				break;
#endif

			case 'd':
				is_filter_display = true;
				break;
			case 'j':
//				throw sinsp_exception("json output not yet implemented");

				if(event_buffer_format != sinsp_evt::PF_NORMAL)
				{
					fprintf(stderr, "you cannot specify more than one output format\n");
					delete inspector;
					return EXIT_SUCCESS;
				}

				event_buffer_format = sinsp_evt::PF_JSON;
				break;
			case 'h':
				usage();
				delete inspector;
				return EXIT_SUCCESS;
			case 'l':
				list_flds = true;
				break;
			case 'L':
				list_events(inspector);
				delete inspector;
				return EXIT_SUCCESS;
			case 'n':
				cnt = atoi(optarg);
				if(cnt <= 0)
				{
					throw sinsp_exception(string("invalid packet count") + optarg);
					res = EXIT_FAILURE;
					goto exit;
				}
				break;
			case 'P':
				print_progress = true;
				break;
			case 'p':
				if(string(optarg) == "p")
				{
					//
					// -pp shows the default output format, useful if the user wants to tweak it.
					//
					replace_in_place(output_format, "<TIME>", timefmt);
					printf("%s\n", output_format.c_str());
					delete inspector;
					return EXIT_SUCCESS;
				}
				else
				{
					output_format = optarg;
				}

				break;
			case 'q':
				quiet = true;
				break;
			case 'r':
				infiles.push_back(optarg);
				break;
			case 'S':
				summary_table = new vector<summary_table_entry>;

				for(uint32_t j = 0; j < PPM_EVENT_MAX; j++)
				{
					summary_table->push_back(summary_table_entry(j, false));
				}

				for(uint32_t j = 0; j < PPM_SC_MAX * 2; j++)
				{
					summary_table->push_back(summary_table_entry(j, true));
				}

				break;
			case 's':
				snaplen = atoi(optarg);
				break;
			case 't':
				{
					string tms(optarg);

					if(tms == "h")
					{
						timefmt = "%evt.time";
					}
					else if(tms == "a")
					{
						timefmt = "%evt.rawtime.s.%evt.rawtime.ns";
					}
					else if(tms == "r")
					{
						timefmt = "%evt.reltime.s.%evt.reltime.ns";
					}
					else if(tms == "d")
					{
						timefmt = "%evt.latency.s.%evt.latency.ns";
					}
				}
				break;
			case 'v':
				verbose = true;
				break;
			case 'w':
				outfile = optarg;
				quiet = true;
				break;
			case 'x':
				if(event_buffer_format != sinsp_evt::PF_NORMAL)
				{
					fprintf(stderr, "you cannot specify more than one output format\n");
					delete inspector;
					return EXIT_SUCCESS;
				}

				event_buffer_format = sinsp_evt::PF_HEX;
				break;
			case 'X':
				if(event_buffer_format != sinsp_evt::PF_NORMAL)
				{
					fprintf(stderr, "you cannot specify more than one output format\n");
					delete inspector;
					return EXIT_SUCCESS;
				}

				event_buffer_format = sinsp_evt::PF_HEXASCII;
				break;
			case 'z':
				compress = true;
				break;
			default:
				break;
			}
		}

		inspector->set_buffer_format(event_buffer_format);

		//
		// If -l was specified, print the fields and exit
		//
		if(list_flds)
		{
			if(verbose)
			{
				//
				// -ll shows the fields verbosely, i.e. with more information
				// like the type
				//
				list_fields(true);
			}
			else
			{
				list_fields(false);
			}

			res = EXIT_SUCCESS;
			goto exit;
		}

		string filter;

		//
		// the filter is at the end of the command line
		//
		if(optind + n_filterargs < argc)
		{
#ifdef HAS_FILTERING
			for(int32_t j = optind + n_filterargs; j < argc; j++)
			{
				filter += argv[j];
				if(j < argc)
				{
					filter += " ";
				}
			}

			if(is_filter_display)
			{
				display_filter = new sinsp_filter(inspector, filter);
			}
#else
			fprintf(stderr, "filtering not compiled.\n");
			res = EXIT_FAILURE;
			goto exit;
#endif
		}

		if(signal(SIGINT, signal_callback) == SIG_ERR)
		{
			fprintf(stderr, "An error occurred while setting SIGINT signal handler.\n");
			res = EXIT_FAILURE;
			goto exit;
		}

		if(signal(SIGTERM, signal_callback) == SIG_ERR)
		{
			fprintf(stderr, "An error occurred while setting SIGTERM signal handler.\n");
			res = EXIT_FAILURE;
			goto exit;
		}

		//
		// Insert the right time format based on the -t flag
		//
		replace_in_place(output_format, "<TIME>", timefmt);

		//
		// Create the event formatter
		//
		sinsp_evt_formatter formatter(inspector, output_format);

		for(uint32_t j = 0; j < infiles.size() || infiles.size() == 0; j++)
		{
#ifdef HAS_FILTERING
			if(filter.size() && !is_filter_display)
			{
				inspector->set_filter(filter);
			}
#endif

			//
			// Launch the capture
			//
			bool open_success = true;

			if(infiles.size() != 0)
			{
				initialize_chisels();

				//
				// We have a file to open
				//
				inspector->open(infiles[j]);
			}
			else
			{
				if(j > 0)
				{
					break;
				}

				initialize_chisels();

				//
				// No file to open, this is a live capture
				//
#if defined(HAS_CAPTURE)
				if(print_progress)
				{
					fprintf(stderr, "the -P flag cannot be used with live captures.\n");
					res = EXIT_FAILURE;
					goto exit;
				}

				try
				{
					inspector->open("");
				}
				catch(sinsp_exception e)
				{
					open_success = false;
				}
#else
				//
				// Starting live capture
				// If this fails on Windows and OSX, don't try with any driver
				//
				inspector->open("");
#endif

				//
				// Starting the live capture failed, try to load the driver with
				// modprobe.
				//
				if(!open_success)
				{
					open_success = true;

					try
					{
						system("modprobe sysdig-probe > /dev/null 2> /dev/null");

						inspector->open("");
					}
					catch(sinsp_exception e)
					{
						open_success = false;
					}
				}

				//
				// No luck with modprobe either.
				// Maybe this is a version of sysdig that was compiled from the
				// sources, so let's make one last attempt with insmod and the
				// path to the driver directory.
				//
				if(!open_success)
				{
					system("insmod ../../driver/sysdig-probe.ko > /dev/null 2> /dev/null");

					inspector->open("");
				}
			}

			if(snaplen != 0)
			{
				inspector->set_snaplen(snaplen);
			}

			duration = ((double)clock()) / CLOCKS_PER_SEC;

			if(outfile != "")
			{
				inspector->autodump_start(outfile, compress);
			}

			//
			// Notify the chisels that the capture is starting
			//
			chisels_on_capture_start();

			cinfo = do_inspect(inspector,
				cnt,
				quiet,
				absolute_times,
				print_progress,
				display_filter,
				summary_table,
				&formatter);

			duration = ((double)clock()) / CLOCKS_PER_SEC - duration;

			scap_stats cstats;
			inspector->get_capture_stats(&cstats);

			if(verbose)
			{
				fprintf(stderr, "Driver Events:%" PRIu64 "\nDriver Drops:%" PRIu64 "\n",
					cstats.n_evts,
					cstats.n_drops);

				fprintf(stderr, "Elapsed time: %.3lf, Captured Events: %" PRIu64 ", %.2lf eps\n",
					duration,
					cinfo.m_nevts,
					(double)cinfo.m_nevts / duration);
			}

			//
			// Done. Close the capture.
			//
			inspector->close();

		}
	}
	catch(sinsp_exception& e)
	{
		cerr << e.what() << endl;
		res = EXIT_FAILURE;
	}
	catch(...)
	{
		res = EXIT_FAILURE;
	}

exit:

	//
	// If there's a summary table, sort and print it
	//
	if(summary_table != NULL)
	{
		print_summary_table(inspector, summary_table, 100);
	}

	free_chisels();

	if(inspector)
	{
		delete inspector;
	}

	if(display_filter)
	{
		delete display_filter;
	}

#ifdef _WIN32
	_CrtDumpMemoryLeaks();
#endif

	return res;
}
