/* WSDrawers.rexx -- what an installed WaveSmith looks like on Workbench:
 * SYS:Tools (the drawer's icon, placed by Workbench) and then every drawer
 * the Installer made, one window more at a time, the screen grabbed after
 * each (DEV:Test/arranged-<tag>-<n>.ppm), then all closed again, last
 * first. For test/auto/WaveSmithInstall (tools/arrange_drawers.py lays the
 * drawers out):
 *   RX DEV:Test/WSDrawers.rexx <tag>
 */
parse arg tag .
if tag = '' then tag = 'x'
base = 'SYS:Tools/WaveSmith'
wins.1 = 'SYS:Tools'
wins.2 = base
wins.3 = base'/Help'
wins.4 = base'/Help/english'
wins.5 = base'/Help/deutsch'
count = 5
log = 'DEV:Test/auto.log'
if ~open('out', log, 'A') then exit 10
do n = 1 to count
	address WORKBENCH 'WINDOW "'wins.n'" OPEN'
	if rc ~= 0 then call writeln('out', '  WSDrawers: cannot open' wins.n '(rc' rc')')
	address command 'Wait 2'
	address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/arranged-'tag'-'n'.ppm'
end
call writeln('out', '  drawers on Workbench ('tag'): arranged-'tag'-1..'count'.ppm')
do n = count to 1 by -1
	address WORKBENCH 'WINDOW "'wins.n'" CLOSE'
end
call close('out')
exit 0
