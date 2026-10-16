# build.tcl - out-of-context synthesis and implementation of one R4 design on the
# Spartan-7 XC7S25 (Arty S7-25), with area, timing and power reports.
#
#   vivado -mode batch -source build.tcl -tclargs <tag> <top> <generics...>
#   e.g.  -tclargs nca5_A nca5_top ARCH=1 D=8
#
# Pass 1 runs at an aggressive clock (4 ns) to find the reachable period.
# Pass 2 re-runs at that period, rounded up, and its results are reported.
# Power is reported twice: at the pass-2 clock and at a common 100 MHz clock,
# with Vivado's default (vectorless) switching activity.
set tag   [lindex $argv 0]
set top   [lindex $argv 1]
set gens  [lrange $argv 2 end]
set part  xc7s25csga324-1
set here  [file dirname [file normalize [info script]]]
set rtl   [file normalize $here/../rtl]
set out   [file normalize $here/reports/$tag]
file mkdir $out
set_param general.maxThreads 8

proc run_pass {period passdir} {
    global top gens part rtl out
    file mkdir $passdir
    set xdc $passdir/clk.xdc
    set f [open $xdc w]; puts $f "create_clock -period $period -name clk \[get_ports clk\]"; close $f
    close_project -quiet
    create_project -in_memory -part $part
    read_vhdl -vhdl2008 [list $rtl/r4_pkg.vhd $rtl/polyval_mac.vhd $rtl/aead1_ctrl.vhd \
        $rtl/ksg_aes.vhd $rtl/ksg_snow.vhd $rtl/ksg_zuc.vhd $rtl/nca5_top.vhd $rtl/nca4_top.vhd $rtl/nca6_top.vhd]
    read_xdc -mode out_of_context $xdc
    set g {}
    foreach x $gens { lappend g -generic $x }
    synth_design -top $top -part $part -mode out_of_context {*}$g
    opt_design
    place_design
    phys_opt_design
    route_design
    set p [get_timing_paths -delay_type max -max_paths 1]
    set wns [get_property SLACK $p]
    if {$wns eq ""} { set wns 0 }
    return $wns
}

# pass 1
set wns1 [run_pass 4.000 $out/pass1]
set reach [expr {4.000 - $wns1}]
set p2 [expr {ceil($reach * 20.0) / 20.0 + 0.05}]
# pass 2
set wns2 [run_pass $p2 $out/pass2]
set fmax [expr {1000.0 / ($p2 - $wns2)}]

report_utilization -file $out/utilization.rpt
report_utilization -hierarchical -file $out/utilization_hier.rpt
report_timing_summary -file $out/timing.rpt
report_power -file $out/power_at_fmax.rpt
set pw1 [report_power -return_string]
create_clock -period 10.000 -name clk [get_ports clk]
report_power -file $out/power_100mhz.rpt
set pw2 [report_power -return_string]
set ut [report_utilization -return_string]

proc grab {re text} { if {[regexp $re $text -> v]} { return $v } else { return "" } }
set lut  [grab {\|\s*Slice LUTs\*?\s*\|\s*(\d+)} $ut]
set ff   [grab {\|\s*Slice Registers\s*\|\s*(\d+)} $ut]
set slc  [grab {\|\s*Slice\s*\|\s*(\d+)} $ut]
set bram [grab {\|\s*Block RAM Tile\s*\|\s*([0-9.]+)} $ut]
set dsp  [grab {\|\s*DSPs\s*\|\s*(\d+)} $ut]
set tot1 [grab {Total On-Chip Power \(W\)\s*\|\s*([0-9.]+)} $pw1]
set dyn1 [grab {Dynamic \(W\)\s*\|\s*([0-9.]+)} $pw1]
set tot2 [grab {Total On-Chip Power \(W\)\s*\|\s*([0-9.]+)} $pw2]
set dyn2 [grab {Dynamic \(W\)\s*\|\s*([0-9.]+)} $pw2]
set sta  [grab {Device Static \(W\)\s*\|\s*([0-9.]+)} $pw2]

set csv [file normalize $here/reports/summary.csv]
if {![file exists $csv]} {
    set f [open $csv w]
    puts $f "tag,top,generics,part,lut,ff,slice,bram,dsp,period_ns,wns_ns,fmax_mhz,total_w_fmax,dyn_w_fmax,total_w_100,dyn_w_100,static_w,vivado"
    close $f
}
set f [open $csv a]
puts $f [join [list $tag $top [join $gens " "] $part $lut $ff $slc $bram $dsp $p2 $wns2 [format %.1f $fmax] $tot1 $dyn1 $tot2 $dyn2 $sta [version -short]] ","]
close $f
puts "R4 RESULT $tag: LUT $lut FF $ff slices $slc BRAM $bram DSP $dsp Fmax [format %.1f $fmax] MHz, dyn power [set dyn2] W at 100 MHz"
