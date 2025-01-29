An interesting program is `taocp/ch1/mixsim/mixsim.mixal`, a MIX meta-simulator to which I've added the ability to load MIX programs from punched cards. To prove that it works, one can try it out with my MIX solution to Day 1 of Advent of Code 2024:

```
> mmm taocp/ch1/mixsim/mixsim.mixal aoc24/1-program.cards
Loaded MIXAL file taocp/ch1/mixsim/mixsim.mixal
Loaded card file aoc24/1-program.cards
MIX Management Module, by wyan
Type h for help
>> g
<now we wait>
```

It will take very long (but not more than a minute), because the original emulator simulates delays in I/O operations, and the delays are compounded in the meta-simulator. I think this will be fixed when I get around to implementing Knuth's suggestion of handling `JBUS *` instructions specially.
