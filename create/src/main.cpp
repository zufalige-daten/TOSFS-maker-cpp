#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string.h>
#include <boost/filesystem.hpp>
#include <errno.h>
typedef unsigned long long ____uint64_t;
#include <stdint.h>
#define uint64_t ____uint64_t
#include <tosfs.h>
#include <string>
#include <time.h>

using namespace std;

typedef struct{
	FILE *RFHandle;
	boost::filesystem::directory_entry BDEntry;
} file_t;

FILE *output_file;
string output_file_str;
vector<file_t> input_files;
vector<file_t> input_directories;

class bound{
private:
	uint64_t start_bytes;
	uint64_t end_bytes;
public:
	bound(void){
		start_bytes = 0;
		end_bytes = 0;
	}
	bound(uint64_t _start_bytes, uint64_t _end_bytes){
		start_bytes = _start_bytes;
		end_bytes = _end_bytes;
	}
	uint64_t get_size_bytes(void){
		return end_bytes - start_bytes;
	}
	uint64_t get_size_sectors(void){
		return (get_size_bytes()/512);
	}
	uint64_t get_size_blocks(void){
		return (get_size_sectors()/4);
	}
	uint64_t get_start_bytes(void){
		return start_bytes;
	}
	uint64_t get_start_sectors(void){
		return (get_start_bytes()/512);
	}
	uint64_t get_end_bytes(void){
		return end_bytes;
	}
	uint64_t get_end_sectors(void){
		return (get_end_bytes()/512);
	}
	void set_start_bytes(uint64_t l){
		start_bytes = l;
	}
	void set_start_sectors(uint64_t l){
		set_start_bytes(l*512);
	}
	void set_end_bytes(uint64_t l){
		end_bytes = l;
	}
	void set_end_sectors(uint64_t l){
		set_end_bytes(l*512);
	}
};

class lbawriter{
private:
	FILE *file;
	uint64_t sizesect;
public:
	lbawriter(void){
		sizesect = 0;
	}
	lbawriter(FILE *_file, uint64_t _sizeinsectors){
		file = _file;
		sizesect = _sizeinsectors;
	}
	void writesect(uint64_t lba, uint8_t *bytebuffer){
		fseeko64(file, lba * 512, SEEK_SET);
		for(uint64_t i = 0; i < 512; i++){
			fputc(bytebuffer[i], file);
		}
	}
	uint8_t *readsect(uint64_t lba, uint8_t *bytebuffer){
		fseeko64(file, lba * 512, SEEK_SET);
		for(uint64_t i = 0; i < 512; i++){
			bytebuffer[i] = fgetc(file);
		}
		return bytebuffer;
	}
	void writeblock(uint64_t base_lba, uint64_t block, uint8_t *bytebuffer){
		uint64_t lba = base_lba + (block * 4);
		fseeko64(file, lba * 512, SEEK_SET);
		for(uint64_t i = 0; i < 4096; i++){
			fputc(bytebuffer[i], file);
		}
	}
	uint8_t *readblock(uint64_t base_lba, uint64_t block, uint8_t *bytebuffer){
		uint64_t lba = base_lba + (block * 4);
		fseeko64(file, lba * 512, SEEK_SET);
		for(uint64_t i = 0; i < 4096; i++){
			bytebuffer[i] = fgetc(file);
		}
		return bytebuffer;
	}
};

FILE *bootloader_file;
uint8_t bootloader_bootsector[512];
FILE *extended_bootloader;
uint64_t extebldr_size = 0;
uint8_t *extended_bootloader_bootsectors;
uint64_t part_size = 0;
string part_name = "NO LABEL   ";
uint64_t reservedsector_count = 2;
uint64_t partstart_lba = 0;

bound b_filesystem;
bound b_reserved;
bound b_bootloader;
bound b_header;
bound b_extbootloader;
bound b_blockalloc;
bound b_data;

lbawriter output;

void garbageday(void){
	if(((uint64_t)extended_bootloader_bootsectors) != 0){
		free(extended_bootloader_bootsectors);
	}
	if(((uint64_t)bootloader_file) != 0){
		fclose(bootloader_file);
	}
	if(((uint64_t)extended_bootloader) != 0){
		fclose(extended_bootloader);
	}
	if(((uint64_t)output_file) != 0){
		fclose(output_file);
	}
	for(file_t fl : input_files){
		if(((uint64_t)(fl.RFHandle)) != 0){
			fclose(fl.RFHandle);
		}
	}
}

void exit_garb(int n){
	garbageday();
	exit(n);
}

void expect(bool cond, std::string msg){
	if(!cond){
		std::cout << "ERROR: " << msg << "\n";
		exit_garb(-1);
	}
}

FILE *fopen_advance(const char *fname, const char *foptions){
	errno = 0;
	FILE *ret = fopen64(fname, foptions);
	expect(errno != EPERM, "Cannot open file or device '" + string(fname) + "'. Operation not permitted.");
	expect(errno != EACCES, "Cannot open file or device '" + string(fname) + "'. Permission denied.");
	expect(errno != EBUSY, "Cannot open file or device '" + string(fname) + "'. Device or resource busy.");
	expect(errno != ENOENT, "Cannot open file or device '" + string(fname) + "'. No such file or directory.");
	errno = 0;
	return ret;
}

void arguments(int argc, char **argv){
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "-b") == 0){
			i++;
			expect(i < argc, "At '-b' in arguments: bootloader file must be specified (-b [bootloader file]).");
			bootloader_file = fopen_advance(argv[i], "rb");
			fread(bootloader_bootsector, 1, 512, bootloader_file);
			fclose(bootloader_file);
		}
		else if(strcmp(argv[i], "-e") == 0){
			i++;
			expect(i < argc, "At '-e' in arguments: extended bootloader sectors file must be specified (-e [extended bootloader sectors file]).");
			extended_bootloader = fopen_advance(argv[i], "rb");
			int size;
			fseeko64(extended_bootloader, 0, SEEK_END);
			size = ftello64(extended_bootloader);
			fseeko64(extended_bootloader, 0, SEEK_SET);
			extebldr_size = (size/512)+(((size%512) > 0) ? 1 : 0);
			extebldr_size *= 512;
			extended_bootloader_bootsectors = (uint8_t *)malloc(extebldr_size);
			fread(extended_bootloader_bootsectors, 1, extebldr_size, extended_bootloader);
			fclose(extended_bootloader);
		}
		else if(strcmp(argv[i], "-s") == 0){
			i++;
			expect(i < argc, "At '-s' in arguments: partition size must be specified (-s [partition size]): [any size] := [number (integer value in decimal)][unit spec :: b = bytes; k = kibibytes; m = mebibytes; g = gibibytes;].");
			char ddr4[strlen(argv[i]) - 1];
			int y = 0;
			for(; y < strlen(argv[i]) - 1; y++){
				ddr4[y] = argv[i][y];
			}
			ddr4[y] = 0;
			uint64_t sz = stoull(ddr4);
			uint64_t multiplier;
			expect(sz > 0, "At '-s' in arguments: partition size must be greater than 0.");
			switch(argv[i][strlen(argv[i])-1]){
				case 'b':
					multiplier = 1;
					break;
				case 'k':
					multiplier = 1024;
					break;
				case 'm':
					multiplier = 1024*1024;
					break;
				case 'g':
					multiplier = 1024*1024*1024;
					break;
				default:
					expect(false, string("At '-s' in arguments: partition size invalid unit. '") + argv[i][y] + "'.");
					break;
			}
			part_size = sz * multiplier;
		}
		else if(strcmp(argv[i], "-l") == 0){
			i++;
			expect(i < argc, "At '-l' in arguments: partition name (volume label) must be specified.");
			part_name = argv[i];
			expect(part_name.size() <= 11, "At '-l' in arguments: partition name (volume label) must not be larger than 11 characters (bytes) in size.");
			bool switchover = false;
			for(int d = 0; d < 11; d++){
				if(switchover){
					part_name += " ";
				}
				else if(part_name[d] == 0){
					part_name += " ";
					switchover = true;
				}
			}
		}
		else if(strcmp(argv[i], "-r") == 0){
			i++;
			expect(i < argc, "At '-r' in arguments: number of reserved sectors must be specified.");
			uint64_t sz = stoull(argv[i]);
			expect(sz >= 2, "At '-r' in arguments: number of reserved sectors must be at least 2.");
			reservedsector_count = sz;
		}
		else if(strcmp(argv[i], "-S") == 0){
			i++;
			expect(i < argc, "At '-S' in arguments: LBA of start of partition must be specified.");
			uint64_t lba = stoull(argv[i]);
			partstart_lba = lba;
		}
		else if(strcmp(argv[i], "-h") == 0){
			cout <<
			"Usage: create.tosfs [output file or device] [arguments].\n"
			"[ -b [bootloader file] \\ -e [extended bootloader file] \\ -s [partition size] \\ -l [partition name] \\ -r [number of reserved sectors] \\ -d [directory of filesystem root] \\ -S [starting LBA of partition] \\ -h ]\n"
			;
			exit_garb(0);
		}
		else if(strcmp(argv[i], "-d") == 0){
			i++;
			expect(i < argc, "At '-d' in arguments: directory to source files from into the root of the partition within the file output must be specified.");
			string dirname = string(argv[i]);
			boost::filesystem::recursive_directory_iterator root_dir(dirname);
			for(auto & entry : root_dir){
				if(boost::filesystem::is_directory(entry)){
					file_t iiret;
					iiret.BDEntry = entry;
					input_directories.push_back(iiret);
				}
				else{
					file_t iiret;
					iiret.BDEntry = entry;
					iiret.RFHandle = fopen_advance(entry.path().string().c_str(), "rb");
					input_files.push_back(iiret);
				}
			}
		}
		else{
			output_file_str = string(argv[i]);
			output_file = fopen_advance(argv[i], "rb+");
		}
	}
}

uint64_t filesize(const char *filename){
	uint64_t ret;
	FILE *file = fopen_advance(filename, "rb");
	fseeko64(file, 0, SEEK_END);
	ret = ftello64(file);
	fclose(file);
	return ret;
}

void calculate_bounds(void){
	b_filesystem.set_start_sectors(partstart_lba);
	b_filesystem.set_end_sectors(partstart_lba + (part_size/512));
	b_reserved.set_start_sectors(partstart_lba);
	b_reserved.set_end_sectors(partstart_lba + reservedsector_count);
	b_bootloader.set_start_sectors(partstart_lba);
	b_bootloader.set_end_sectors(partstart_lba + 1);
	b_header.set_start_sectors(partstart_lba + 1);
	b_header.set_end_sectors(partstart_lba + 2);
	b_extbootloader.set_start_sectors(partstart_lba + 2);
	b_extbootloader.set_end_sectors(partstart_lba + reservedsector_count);
	b_blockalloc.set_start_sectors(b_extbootloader.get_end_sectors());
	b_blockalloc.set_end_sectors(b_blockalloc.get_start_sectors() + ((uint64_t)(((uint64_t)(((part_size/512)-reservedsector_count))<<9))>>23));
	b_data.set_start_sectors(b_blockalloc.get_end_sectors());
	b_data.set_end_sectors(part_size/512);
}

sector_t header_sector;
TOSFS10Header_t *header = (TOSFS10Header_t *)header_sector;

void allocate_block(uint64_t block, uint8_t type){
	uint64_t lba = b_blockalloc.get_start_sectors()+((block/4)/512);
	uint64_t byte = ((block/4)%512);
	uint64_t half_nybble = (block%4);
	TOSFS10BlockAllocRegionEntryGroup_t temp[512];
	output.readsect(lba, (uint8_t *)temp);
	switch(half_nybble){
		case 0:
			temp[byte]._0 = type;
			break;
		case 1:
			temp[byte]._1 = type;
			break;
		case 2:
			temp[byte]._2 = type;
			break;
		case 3:
			temp[byte]._3 = type;
			break;
		default:
			expect(false, "alloc_block_error_1. (report in issues).");
			break;
	}
	output.writesect(lba, (uint8_t *)temp);
}

uint64_t get_lowest_block(uint8_t type){
	bool avaliable = false;
	uint64_t ret = 0;
	TOSFS10BlockAllocRegionEntryGroup_t temp[512];
	for(int lba_i = 0; lba_i < b_blockalloc.get_size_sectors(); lba_i++){
		output.readsect(b_blockalloc.get_start_sectors() + lba_i, (uint8_t *)temp);
		for(int byte_i = 0; byte_i < 512; byte_i++){
			if(temp[byte_i]._0 == type){
				ret = (lba_i * (512 * 4))+(byte_i * 4);
				avaliable = true;
				break;
			}
			else if(temp[byte_i]._1 == type){
				ret = (lba_i * (512 * 4))+(byte_i * 4)+1;
				avaliable = true;
				break;
			}
			else if(temp[byte_i]._2 == type){
				ret = (lba_i * (512 * 4))+(byte_i * 4)+2;
				avaliable = true;
				break;
			}
			else if(temp[byte_i]._3 == type){
				ret = (lba_i * (512 * 4))+(byte_i * 4)+3;
				avaliable = true;
				break;
			}
		}
		if(avaliable){
			break;
		}
	}
	return ret;
}

uint64_t create_file(const char *file_name, uint8_t file_attributes, TOSFS10FilePermsDataBlock_t *file_permissions){
	uint64_t block = 0;
	if(header->LFAEBlock == -1){
		block = get_lowest_block(TOSFS10BlockAllocType::Unused);
		allocate_block(block, TOSFS10BlockAllocType::FODANFull);
		header->LFAEBlock = block;
	}
	else{
		block = header->LFAEBlock;
	}
	uint64_t perms_block = get_lowest_block(TOSFS10BlockAllocType::Unused);
	allocate_block(perms_block, TOSFS10BlockAllocType::FODData);
	TOSFS10FileAllocEntryBlock_t file_entry;
	bool success = false;
	uint64_t success_lba = 0;
	bool free = false;
	for(int i = 0; i < 4; i++){
		output.readsect(b_data.get_start_sectors() + (block * 4) + i, (uint8_t *)&file_entry.FAEntries[i]);
		if(file_entry.FAEntries[i].FAttributes == 0){
			if(success){
				free = true;
			}
			else{
				file_entry.FAEntries[i].FAttributes = TOSFS10FileAttributes::FAEUsed | file_attributes;
				success_lba = b_data.get_start_sectors() + (block * 4) + i;
				file_entry.FAEntries[i].NTerminator = 0;
				file_entry.FAEntries[i].FPSBlock = perms_block;
				file_entry.FAEntries[i].FSBlock = -1;
				file_entry.FAEntries[i].FSIBytes = 0; // used for number of entries in directory if the file is a directory
				memcpy(file_entry.FAEntries[i].FName, file_name, strlen(file_name));
				uint64_t sf = strlen(file_name);
				for(int q = sf; q < 466; q++){
					file_entry.FAEntries[i].FName[q] = 0;
				}
				int32_t unixtime = time(NULL);
				file_entry.FAEntries[i].FCTime = unixtime;
				file_entry.FAEntries[i].FLMTime = unixtime;
				file_entry.FAEntries[i].FLRTime = unixtime;
				success = true;
			}
		}
	}
	if(!free){
		uint64_t n_block = get_lowest_block(TOSFS10BlockAllocType::Unused);
		allocate_block(n_block, TOSFS10BlockAllocType::FODANFull);
		header->LFAEBlock = n_block;
	}
	output.writeblock(b_data.get_start_sectors(), block, (uint8_t *)file_entry.FAEntries);
	output.writeblock(b_data.get_start_sectors(), perms_block, (uint8_t *)file_permissions);
	return success_lba;
}

void add_dir_entry(uint64_t directory_lba, uint64_t file_lba){
	_TOSFS10FileAllocEntry_t directory_entry;
	_TOSFS10FileAllocEntry_t file_entry;
	output.readsect(directory_lba, (uint8_t *)&directory_entry);
	output.readsect(file_lba, (uint8_t *)&file_entry);
}

void create_partition(void){
	// bootloader
	if(bootloader_file != (FILE *)0){
		output.writesect(b_bootloader.get_start_sectors(), (uint8_t *)bootloader_bootsector);
	}
	// extended boot loader
	if(extended_bootloader_bootsectors != (uint8_t *)0){
		int sector_count = reservedsector_count - 2;
		for(int i = 0; i < sector_count; i++){
			output.writesect(b_extbootloader.get_start_sectors() + i, ((uint8_t *)(&extended_bootloader_bootsectors[i*512])));
		}
	}
	header->PSILba = part_size/512;
	memcpy(header->Sig, "TOSFS\x00\x00\x00", 8);
	header->LFAEBlock = -1;
	header->BARSLba = b_blockalloc.get_start_sectors();
	header->BSLba = b_data.get_start_sectors();
	header->LBAOBOPart = b_filesystem.get_start_sectors();
	memcpy(header->PName, part_name.c_str(), 11);
	header->RSCount = reservedsector_count;
	header->BARSSec = b_blockalloc.get_size_sectors();
	header->VNumber = 10;
	header->RDSLba = b_data.get_start_sectors();
	// block allocation region
	// nothing to do here as it is filled up with the following structures
	// file data region (initially root directory entry and also it's local structures)
	// root directory file entry
	TOSFS10FilePermsDataBlock_t file_permissions;
	memset(&file_permissions, 'P', 4096);
	file_permissions.MSPerms = TOSFS10Perms::CExecute | TOSFS10Perms::CRead | TOSFS10Perms::Overwrite;
	file_permissions.GUPerms = TOSFS10Perms::CExecute | TOSFS10Perms::CRead;
	file_permissions.PEntries[0].Nullterm = 0;
	file_permissions.PEntries[0].Perms = 0;
	memset(file_permissions.PEntries[0].NAGOGroup, 0, 23);
	uint64_t rdslba = create_file("", TOSFS10FileAttributes::IFDir, &file_permissions);
	header->RDSLba = rdslba;
	uint64_t f0 = create_file("TestFile1.txt", 0, &file_permissions);
	uint64_t f1 = create_file("Foo.Bar", 0, &file_permissions);
	add_dir_entry(rdslba, f0);
	add_dir_entry(rdslba, f1);
	// header
	output.writesect(b_header.get_start_sectors(), (uint8_t *)header_sector);
}

int main(int argc, char **argv){
	expect(argc >= 1, "Not enough arguments for command: create.tosfs [output file or device] [arguments].");
	output_file = (FILE *)0;
	extended_bootloader_bootsectors = (uint8_t *)0;
	arguments(argc, argv);
	expect((uint64_t)output_file != 0, "Output image file to write to / device is missing from arguments.");
	for(file_t fl : input_directories){
		int i = fl.BDEntry.path().string().find_first_of('/', 0);
		string res = fl.BDEntry.path().string().substr(i, fl.BDEntry.path().string().size()-i);
		fl.BDEntry.assign(res);
	}
	for(file_t fl : input_files){
		int i = fl.BDEntry.path().string().find_first_of('/', 0);
		string res = fl.BDEntry.path().string().substr(i, fl.BDEntry.path().string().size()-i);
		fl.BDEntry.assign(res);
	}
	expect(part_size > 0 && (part_size%512) == 0, "Partition size must be larger than 0 and a multiple of 512.");
	expect(filesize(output_file_str.c_str()) >= (partstart_lba*512)+part_size, "Output file / device must have a total size equal to or greater than the expected partition final lba multiplied by 512 (output file / device too small in size).");
	calculate_bounds();
	output = lbawriter(output_file, partstart_lba+(part_size/512));
	create_partition();
	exit_garb(0);
	return 0;
}
