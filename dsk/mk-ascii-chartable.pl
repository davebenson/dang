#! /usr/bin/perl -w

my @tqble = ();
for (my $i = 0; $i < 256; $i++) { push @table, 0 }

# set spaces
for (' ', "\t", "\n") { $table[ord($_)] |= 1 }

# set isalpha bits
for ('a'..'z', 'A'..'Z') { $table[ord($_)] |= 2 }
# set isdigit bits
for ('0'..'9') { $table[ord($_)] |= 4 }
# set isxdigit bits
for ('0'..'9', 'a'..'f', 'A'..'F') { $table[ord($_)] |= 8 }
# set isalpha bits
for ('a'..'z', 'A'..'Z') { $table[ord($_)] |= 2 }

for (my $i = 0; $i < 256; $i++) { print sprintf("0x%02x,", $table[$i]); if ($i % 16 == 15) {print "\n"} }
