import thread, re, time, socket;
import getopt, shlex;
import datetime;
import sys
from client import GEMINIDB, GEMINIDB_Response;
import readline;

escape_data = False;

host = '';
port = '';
opt = '';
args = [];


def welcome():
    sys.stderr.write('geminidb (cli) - geminidb command line tool.\n');
    sys.stderr.write('Copyright (c) 2012-2016 geminidb.io\n');
    sys.stderr.write('\n');
    sys.stderr.write("'h' or 'help' for help, 'q' to quit.\n");
    sys.stderr.write('\n');


def show_command_help():
    print '# press \'info\' for displaying geminidb-server status';
    print '# press \'q\' and Enter to quit.';
    print '';



def usage():
    print '';
    print 'Usage:';
    print '	geminidb-cli [-h] [HOST] [-p] [PORT]';
    print '';
    print 'Options:';
    print '	-h 127.0.0.1';
    print '	        server hostname/ip address of geminidb';
    print '	-p 8888';
    print '		server port of geminidb';
    print '	-v --help';
    print '		show help message';
    print 'Examples:';
    print '	geminidb-cli';
    print '	geminidb-cli -h 127.0.0.1 -p 8888';


def repr_data(s):
    gs = globals();
    if gs['escape_data'] == False:
        return s;
    ret = str(s).encode('string-escape');
    return ret;


def timespan(stime):
    etime = datetime.datetime.now();
    ts = etime - stime;
    time_consume = ts.seconds + ts.microseconds / 1000000.;
    return time_consume;


def request_with_retry(cmd, args=None):
    gs = globals();
    link = gs['link'];
    if not args:
        args = [];
    retry = 0;
    max_retry = 5;
    while True:
        resp = link.request(cmd, args);
        if resp.code == 'disconnected':
            link.close();
            sleep = retry;
            if (sleep > 3):
                sleep = 3;
            time.sleep(sleep);
            retry += 1
            if retry > max_retry:
                sys.stderr.write('cannot connect to server, give up...\n');
                break;
            sys.stderr.write('[%d/%d] reconnecting to server... ' % (retry, max_retry));
            try:
                link = GEMINIDB(host, port);
                gs['link'] = link;
                sys.stderr.write('done.\n');
            except socket.error, e:
                sys.stderr.write('Connect error: %s\n' % str(e));
                continue;
        else:
            return resp;
    return None;


if __name__ == '__main__':
    for arg in sys.argv:
        if opt == '' and arg.startswith('-'):
            opt = arg;
            if arg == '--help' or arg == '--h' or arg == '-v':
                usage();
                exit(0)
    if host == '':
        host = '127.0.0.1'
    if port == '':
        port = int('8888')
    try:
        link = GEMINIDB(host, port);
    except socket.error, e:
        sys.stderr.write('Failed to connect to: %s:%d\n' % (host, port));
        sys.stderr.write('Connection error: %s\n' % str(e));
        sys.exit(0);
    welcome();
    while True:
        line = '';
        c = 'geminidb %s:%s> ' % (host, str(port));
        b = sys.stdout;
        sys.stdout = sys.stderr;
        try:
            line = raw_input(c)
        except Exception, e:
            break;
        sys.stdout = b;
        if line == '':
            continue;
        line = line.strip();
        if line == 'q' or line == 'quit':
            sys.stderr.write('bye.\n');
            break;
        if line == 'h' or line == 'help':
            show_command_help();
            continue;
        try:
            ps = shlex.split(line);
        except Exception, e:
            sys.stderr.write('error: %s\n' % str(e));
            continue;
        if len(ps) == 0:
            continue;
        for i in range(len(ps)):
            ps[i] = ps[i].decode('string-escape');
        cmd = ps[0].lower();
        if cmd.startswith(':'):
            ps[0] = cmd[1:]
            cmd = ':';
            args = ps;
        else:
            args = ps[1:];
        if cmd == ':':
            op = '';
            if len(args) > 0:
                op = args[0];
            if op != 'escape':
                sys.stderr.write("Bad setting!\n");
                continue;
            yn = 'yes';
            if len(args) > 1:
                yn = args[1];
            gs = globals();
            if yn == 'yes':
                gs['escape_data'] = True;
                sys.stderr.write("  Escape response\n");
            elif yn == 'no' or yn == 'none':
                gs['escape_data'] = False;
                sys.stderr.write("  No escape response\n");
            else:
                sys.stderr.write("  Usage: escape yes|no\n");
            continue;
        if cmd == 'v':
            continue;

        stime = datetime.datetime.now();
        resp = request_with_retry(cmd, args);
        if resp == None:
            sys.stderr.write("error!\n");
            continue;
        time_consume = timespan(stime);

        if not resp.ok():
            if resp.not_found():
                sys.stderr.write('not_found\n');
            else:
                s = resp.code;
                if resp.message:
                    s += ': ' + str(resp.message);
                sys.stderr.write(str(s) + '\n');
            sys.stderr.write('(%.3f sec)\n' % time_consume);
        else:
            # sys.stderr.write('(%.3f sec)\n' % time_consume);
            if resp.type == "none":
                print(str(resp.data) + '\n');
            elif resp.type == 'val':
                if resp.code == 'ok':
                    print(str(resp.data));
                else:
                    if resp.data:
                        print repr_data(resp.code), repr_data(resp.data);
                    else:
                        print repr_data(resp.code);
            elif resp.type == 'list':
                sys.stderr.write('  %15s\n' % 'key');
                sys.stderr.write('-' * 17);
                for k in resp.data:
                    print('  %15s\n', repr_data(k));
                sys.stderr.write('%d result(s) (%.3f sec)\n' % (len(resp.data), time_consume));
            sys.stderr.write('(%.3f sec)\n' % time_consume);
