/* EditAbout.rexx -- the autotest EditAbout's driver: WaveSmith's About
   window (DEV:Test/about.trace says when it opens and shuts).
     A  Project / About opens it ("about open"); the screen grabbed --
        the menu, not Right-Amiga ?: the emulator's keymap is German
     B  About again while it is open: still one ("about open" again, no
        second window)
     C  Esc shuts it ("about closed"); Quit leaves nothing running */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

last. = ''
n. = 0

do i = 1 to 120 while n.PLANNED = 0
	call delay(50)
	call readtrace
end
if n.PLANNED = 0 then do
	call writeln(log, 'FAILED no plan in DEV:Test/about.trace after 60 s')
	exit 20
end

call writeln(log, 'A: Project / About opens the About window')
call about_pick
do i = 1 to 20 while n.ABOUT = 0
	call delay(10)
	call readtrace
end
if last.ABOUT ~= 'open' then call writeln(log, 'FAILED no "about open" in the trace (last:' last.ABOUT')')
else call writeln(log, 'ok   about open')
call delay(50)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-about.ppm'

call writeln(log, 'B: About again brings the same one forward')
a0 = n.ABOUT
call about_pick
do i = 1 to 20 while n.ABOUT = a0
	call delay(10)
	call readtrace
end
if n.ABOUT ~= a0 + 1 | last.ABOUT ~= 'open' then call writeln(log, 'FAILED the second open: "'last.ABOUT'"')
else call writeln(log, 'ok   still open, once')

call writeln(log, 'C: Esc shuts it, Quit')
call delay(30)
a0 = n.ABOUT
/* the About window is the active one again: Esc to it */
address command 'DEV:Test/InputPoke KEY=69'
do i = 1 to 20 while n.ABOUT = a0
	call delay(10)
	call readtrace
end
if last.ABOUT ~= 'closed' then call writeln(log, 'FAILED Esc: no "about closed" (last:' last.ABOUT')')
else call writeln(log, 'ok   about closed')
call delay(30)
address command 'DEV:Test/InputPoke WIN=Single.wav KEY=16 QUAL=128'
call delay(250)
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
if rc = 0 then call writeln(log, 'FAILED Quit: still running')
else call writeln(log, 'ok   Quit')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

/* Project / About through the menu, from the trace's "aboutitem title x
   y <title x> <item x> <item y>": the button pressed over the title, led
   to the item, let go */
about_pick:
	parse var last.ABOUTITEM . . . tx ix iy .
	/* WaveSmith's window active (its title bar clicked): About has no menus */
	address command 'DEV:Test/InputPoke WIN=Single.wav X=200 Y=4'
	call delay(15)
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2 STEPS=4'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	return

/* the trace: each line's first word counted, its rest kept */
readtrace: procedure expose last. n.
	n. = 0
	if ~open(tr, 'DEV:Test/about.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
	end
	call close(tr)
	return
