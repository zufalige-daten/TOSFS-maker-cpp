# "b" = byte(s); "B" = bit(s); "\" : escape sequence equivilent for c
# ALL LBAS mentioned must be treated as local addressing lbas from the point of veiw of the lba the partition starts at (each lba = part_start_lba+lba)
# (1 block = 4096 bytes (++ lbas address in 512 byte sectors)) blocks to lbas (first lba for a block reading lbas until block read) = block(n) = BSLba+lba(n*4096)
# for gpt: insert bootsector and gpt structures before "TOSFS" as a partition but keep the "boot sector" field in the filesystem after the bootsector and gpt structures
# (the "boot sector" field will still be treated as the start of the TOSFS partition exept the value "LBA of begining of partition" needs to be modified accordingly)

"TOSFS" : % = {
	"reserved sectors" : [%, 512b] = {
		"boot sector" : 512b = {
			"boot loader" : 440;
			"mbr" : 70b = {
				"unique disk id (UDId)" : 4b;
				"reserved (Res)" : 2b;
				"partition table entries (PTEntries)" : [4, 16b] = {
					%[n]: "partition table entry (%[n])" : 16b = {
						"drive attributes (DAttribs)" : 1b; # bit 7 set = active or bootable
						"CHS partition start (CHSPStart)" : 3b;
						"partition type (PType)" : 1b;
						"CHS address of last partition sector (CHSAOLPSector)" : 3b;
						"LBA of partition start (LBAOPStart)" : 4b;
						"number of sectors in partition (NOSIPartition)" : 4b;
					}
				}
			}
			"boot signiture magic" : 2b = 0xaa55;
		}
		"fs header" : 512b = {
			"signiture (Sig)" : 8b = "TOSFS\x00\x00\x00";
			"lowest free allocation entry LBA (LFAELba)" : 8b; # (!0): no allocation block with free allocation entries allocated
			"block allocation region start LBA (BARSLba)" : 8b;
			"block start lba (BSLba)" : 8b;
			"partition size in lba (PSILba)" : 8b;
			"LBA of begining of partition (LBAOBOPart)" : 8b;
			"partition name (PName)" : 11b; # any unused bytes must be space characters (' ')
			"reserved sector count (RSCount)" : 2b; # must be at least 2 (to account for the boot sector and the filesystem header)
			"block allocation region size sectors (BARSSec)" : 8b = 1+((((('fs header'.PSILba-'fs header'.RSCount)*512)/16384)+1)/512); # can be optimized as: 1+((((('fs header'.PSILba-'bpb'.RSCount)*512)>>14)+1)>>9)
			"version number (VNumber)" : 2b = 10; # 10 for 1.0
			"root directory start LBA (RDSLba)" : 8b;
			"reserved (Res)" : %;
		}
		"extended boot loader" : [%, 512b];
	}
	"block allocation region" : ('fs header'.BARSSec*512)b = {
		%[n] _peqenno: "entry (%[n])" : 2B; # 0: unused; 1: file or dir allocation (block full); 2: file or dir allocation (block is not full); 3: file data;
	}
	"data blocks" : (('fs header'.PSILba-'fs header'.BSLba)*4096)b = {
		: "file allocation entry block" : [4, 512b] = {
			"file allocation entry" : 512b = {
				"file attributes (FAttributes)" : 1b;
				: '
				"file attributes [bit, explanation]"{
					0: file allocation entry used
					1: is file hidden
					2: is file a span file
					3: is file a directory
					4: is file a symbolic link
				}
				'
				"file last modified time (FLMTime)" : 4b; # sint32_t value that represents the number of non-leap seconds since midnight UTC on 1st January 1970 (the UNIX epoch) aka UNIX time (standered)
				"file last read time (FLRTime)" : 4b;
				"file creation time (FCTime)" : 4b;
				"file size in bytes (FSIBytes)" : 8b; # if is a span file FSIBytes must be a multiple of 4096
				"file permissions start block (FPSBlock)" : 8b;
				"file start block | file symbolic link file entry LBA (FSBlock)" : 8b;
				"file name (FName)" : 466b; # must be null terminated when file name ends (filename includes any file extenstions: excludes the following characters: { '/' '\' '*' OR ':' })
				"null terminator (NTerminator)" : 1b = 0; # null terminator if file name reaches maximum allowed size of: 466 bytes
			}
		}
		| "file data block (dynamic)" : 4096b = {
			"file region file data (FRFData)" : 4088b;
			"next file region block (NFRBlock)" : 8b;
		}
		| "file data block (span)" : 4096b = {
			"file region file data (FRFData)" : 4096b;
		}
		| "directory data block (can only be dynamic if span is enabled it must be ignored)" : 4096b = {
			"dir region dir data (DRDData)" : [240, 17]b = {
				%[n]: "dir region entry (DREntry)" : 17b = {
					"entry used (EUsed)" : 1b; # 0: unused; 1: used
					"file or dir name hash (FODNHash)" : 8b; # sum of all characters as a long long (64 bit integer) including null bytes excluding preallocated null terminator divided by 128 as a double precision float (64 bit float)
					"file or dir entry lba (FODELba)" : 8b;
				}
			}
			"reserved (Rsvd)" : 8b;
			"next dir region block (NDRBlock)" : 8b;
		}
		| "file permissions data block (dynamic or span ignored)" : 4096b = {
			"minimum strictness permissions (MSPerms)" : 8b;
			"global user permissions (GUPerms)" : 8b;
			"permission entries (PEntries)" : [32b, 127] = { # 127 permission entries for users and user groups maximum for each file entry (but not including system set minimum permission strictness entry and the global user permissions entry (the 2 are specially formated entry as they will always show up at the start in that specific order in the file permissions data block)
				"permissions (Perms)" : 8b;
				: '
					[bit] [explanation]
					0: can read
					1: can write
					2: can create
					3: can delete
					4: can execute
					5: overwrite (directory attributes of file overwrite all members of this directory recursivly in that each file and directory is given these permissions or stricter and can only achive their own permissions less strict than these by having their own "overwrite" attribute set) note: overwrite is only valid in MSPerms
				'
				"name at group or group (NAGOGroup)" : 23b; # null terminated if finishing before name end
				"nullterm (Nullterm)" : 1b = 0; # null termination incase name not terminated
			}
		}
	}
}

: '
--	as for the root directory/s (1.0) --
root directory (name = ""; file allocation entry used = 1; hidden = 0; is file a directory = 1;) : ("file allocation entry") :: first entry;
-- expected to be in (1.5) but is possible to be ignored if it is not supported by a 1.0 driver --
circular log (name = "*"; file allocation entry used = 1; hidden = 1; is file a span file = 1;) : ("file allocation entry") :: second entry;
'
