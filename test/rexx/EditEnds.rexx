/* EditEnds.rexx -- the autotest EditTrim's first part: WaveSmith reading
   a long file's ends first (SetEnv WaveSmithEnds 10: the single's first
   and last 10 s; its trace DEV:Test/ends.trace):
     P  the suggestion shown while the rest is still read -- the ends line,
        the plan the whole file's (In 3.6 s, Out 35.88 s, 8 and 4 clicks),
        before the reading's end, the time left said -- and a Save
        (Right-Amiga S) at once, the picture not done: saved, and the doc
        kept -- the trimmed file not read again, its picture made while
        it was written (edit-p)
     Q  Quit (Right-Amiga Q)
   On the host afterwards: make_edit_single.py --check dist/Test/Edit Ends */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

call readtrace
do i = 1 to 300 while plan = ''
	call delay(10)
	call readtrace
end
if plan = '' then do
	call writeln(log, 'FAILED no plan in DEV:Test/ends.trace after 60 s')
	address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-p.ppm'
	exit 20
end
/* at once: the rest is still being read */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
call writeln(log, 'P: ends (head tail)' ends'; plan (in out leadclicks tailclicks)' plan 'at' planned 'ticks;' reads 'readings done')
if ends = '' then call writeln(log, 'FAILED not read ends first: no ends line')
else if ends ~= '441000 1321236' then call writeln(log, 'FAILED the ends: head to and tail from' ends', not 441000 1321236')
else call writeln(log, 'ok   ends first: the head to frame 441000, the tail from 1321236')
if plan ~= '158760 1582308 8 4' then call writeln(log, 'FAILED the plan from the ends:' plan', not the whole file''s 158760 1582308 8 4')
else call writeln(log, 'ok   the plan from the ends: the whole file''s')
if reads ~= 0 then call writeln(log, 'FAILED the plan came with the reading''s end, not before')
else call writeln(log, 'ok   the plan shown, the rest still read')

if timeleft = '' then call writeln(log, 'FAILED the reading said no time left')
else call writeln(log, 'ok   the reading said the time left (' || timeleft 's at first)')

/* the save: the doc kept, the trimmed file not read again */
do i = 1 to 300 while saved = '' | kept = ''
	call delay(10)
	call readtrace
end
call delay(100)
call readtrace
if kept = '' then call writeln(log, 'FAILED the save did not keep the doc (no keptsave line)')
else if word(kept, 1) ~= 1423548 then call writeln(log, 'FAILED the doc kept with' word(kept, 1) 'frames, not 1423548 (Out - In)')
else if reads > 0 then call writeln(log, 'FAILED the trimmed file read again (' || reads 'readings)')
else call writeln(log, 'ok   the doc kept: 1423548 frames, nothing read again')
if gauge ~= '' & gauge ~= 0 then call writeln(log, 'FAILED the gauge after the save at' gauge', not empty')
else call writeln(log, 'ok   the gauge empty after the save')
call writeln(log, 'saved (in out err)' saved 'after' readsatsave 'readings; first reading (frames ticks)' read)
if saved = '' then call writeln(log, 'FAILED no saved line: Right-Amiga S while reading did nothing')
else if saved ~= '158760 1582308 0' then call writeln(log, 'FAILED the save:' saved', not 158760 1582308 0')
else if readsatsave ~= 0 then call writeln(log, 'FAILED the save came after the reading''s end: not while reading (too late?)')
else call writeln(log, 'ok   saved while the rest was read')
parse var read . rticks
if planned ~= '' & datatype(rticks, 'W') then call writeln(log, '     the plan after' planned 'ticks, the picture after' rticks)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-p.ppm'

call writeln(log, 'Q: Quit')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
call delay(100)
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

/* the trace, the first of each line (the trimmed file read again adds
   its own): the ends, the plan, when, the readings done before the save */
readtrace: procedure expose ends plan planned read reads saved readsatsave cbox cancelled timeleft gauge kept
	kept = ''
	ends = ''
	plan = ''
	planned = ''
	read = ''
	reads = 0
	saved = ''
	readsatsave = -1
	cbox = ''
	cancelled = ''
	timeleft = ''
	gauge = ''
	if ~open(tr, 'DEV:Test/ends.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		select
		when what = 'ENDS' & ends = '' then ends = subword(rest, 3, 2)
		when what = 'PLAN' & plan = '' then plan = subword(rest, 5, 4)
		when what = 'PLANNED' & planned = '' then planned = word(rest, 2)
		when what = 'READ' then do
			if read = '' then read = subword(rest, 3, 2)
			reads = reads + 1
		end
		when what = 'CANCELBUTTON' & cbox = '' then cbox = rest
		when what = 'KEPTSAVE' & kept = '' then kept = subword(rest, 4, 3)
		when what = 'CANCELLED' then cancelled = strip(rest)
		when what = 'GAUGE' then gauge = word(rest, 2)
		when what = 'TIMELEFT' & timeleft = '' then timeleft = word(rest, 2)
		when what = 'SAVED' & saved = '' then do
			saved = subword(rest, 4, 3)
			readsatsave = reads
		end
		otherwise nop
		end
	end
	call close(tr)
	return
