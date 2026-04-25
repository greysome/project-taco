To mixsim, I've added the ability to load MIX programs from punched cards. For example, here's how to run my MIX solution to Day 1 of Advent of Code 2024 using mmmmmix (from this directory):

> mmm mixsim/mixsim.mixal ../../aoc24/1-program.cards
Loaded MIXAL file mixsim/mixsim.mixal
Loaded card file ../../misc/aoc24/1-program.cards
MIX Management Module, by wyan
Type h for help
>> g
<now we wait>

If you patiently wait, you'll eventually see the following output:
0001882714
0019437052

(These are the solutions to both parts, using my specific set of inputs.)

It takes very long to run because mmmmmix simulates delays in I/O operations, and the delays are compounded in the meta-simulator. I think this will be fixed when I get around to implementing Knuth's suggestion of handling `JBUS *` instructions specially.