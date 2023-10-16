
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE libstorage

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "storage/SystemInfo/CmdCryptsetup.h"
#include "storage/Utils/Mockup.h"
#include "storage/Utils/StorageDefines.h"
#include "storage/Utils/SystemCmd.h"


using namespace std;
using namespace storage;


void
check(const string& name, const vector<string>& input, const vector<string>& output)
{
    Mockup::set_mode(Mockup::Mode::PLAYBACK);
    Mockup::set_command({ CRYPTSETUP_BIN, "luksDump", name }, input);

    CmdCryptsetupLuksDump cmd_cryptsetup_luks_dump(name);

    ostringstream parsed;
    parsed.setf(std::ios::boolalpha);
    parsed << cmd_cryptsetup_luks_dump;

    string lhs = parsed.str();
    string rhs = boost::join(output, "\n");

    BOOST_CHECK_EQUAL(lhs, rhs);
}


BOOST_AUTO_TEST_CASE(parse1)
{
    vector<string> input = {
	"LUKS header information for /dev/sdc1",
	"",
	"Version:       	1",
	"Cipher name:   	aes",
	"Cipher mode:   	xts-plain64",
	"Hash spec:     	sha256",
	"Payload offset:	4096",
	"MK bits:       	512",
	"MK digest:     	ed dd 26 85 b0 8b e2 3a ae ff 2c 49 7b 7a 71 9c cd 72 cc 96 ",
	"MK salt:       	d5 a1 b4 bd cc fb d4 42 c0 b1 e2 79 00 9a fb 4e ",
	"               	88 97 5f 9f 9e 0c b9 76 d4 6e 64 d8 0a 88 e5 28 ",
	"MK iterations: 	139586",
	"UUID:          	f0b3c940-6bf1-4afa-8ba4-fa4d97b026b6",
	"",
	"Key Slot 0: ENABLED",
	"	Iterations:         	2242942",
	"	Salt:               	4a 80 f7 c2 fe 86 fe 5b 55 52 cc 75 92 68 55 e5 ",
	"	                      	d4 b5 f2 a5 c8 ad 74 d6 01 e7 a5 20 f2 8e c5 d5 ",
	"	Key material offset:	8",
	"	AF stripes:            	4000",
	"Key Slot 1: DISABLED",
	"Key Slot 2: DISABLED",
	"Key Slot 3: DISABLED",
	"Key Slot 4: DISABLED",
	"Key Slot 5: DISABLED",
	"Key Slot 6: DISABLED",
	"Key Slot 7: DISABLED"
    };

    vector<string> output = {
	"name:/dev/sdc1 uuid:f0b3c940-6bf1-4afa-8ba4-fa4d97b026b6 encryption-type:luks1 cipher:aes-xts-plain64 key-size:64"
    };

    check("/dev/sdc1", input, output);
}


BOOST_AUTO_TEST_CASE(parse2)
{
    vector<string> input = {
	"LUKS header information",
	"Version:       	2",
	"Epoch:         	3",
	"Metadata area: 	16384 [bytes]",
	"Keyslots area: 	16744448 [bytes]",
	"UUID:          	c8338763-450d-4143-92b2-dff843aff1ac",
	"Label:         	LUKS-TEST",
	"Subsystem:     	(no subsystem)",
	"Flags:       	(no flags)",
	"",
	"Data segments:",
	"  0: crypt",
	"	offset: 16777216 [bytes]",
	"	length: (whole device)",
	"	cipher: aes-xts-plain64",
	"	sector: 512 [bytes]",
	"",
	"Keyslots:",
	"  0: luks2",
	"	Key:        512 bits",
	"	Priority:   normal",
	"	Cipher:     aes-xts-plain64",
	"	Cipher key: 512 bits",
	"	PBKDF:      argon2i",
	"	Time cost:  9",
	"	Memory:     1048576",
	"	Threads:    4",
	"	Salt:       ab 47 da f9 48 38 ce 38 76 10 01 eb eb 1e 17 d8 ",
	"	            d1 bd 26 f4 a7 21 18 12 23 20 bc 4c 21 f1 cf ea ",
	"	AF stripes: 4000",
	"	AF hash:    sha256",
	"	Area offset:32768 [bytes]",
	"	Area length:258048 [bytes]",
	"	Digest ID:  0",
	"Tokens:",
	"Digests:",
	"  0: pbkdf2",
	"	Hash:       sha256",
	"	Iterations: 140183",
	"	Salt:       93 80 06 cb de f6 c5 a2 a8 71 c0 72 79 17 ed 93 ",
	"	            bc 59 c9 a7 0f 83 33 49 7d 2d e6 8b b3 63 ce e0 ",
	"	Digest:     89 3e cf f2 a2 88 fb 33 f5 35 8f f5 c7 9a aa ec ",
	"	            21 32 bd 30 cc e7 66 6a 68 bc 6b 30 e9 4a 21 63 "
    };

    vector<string> output = {
	"name:/dev/sdc1 uuid:c8338763-450d-4143-92b2-dff843aff1ac encryption-type:luks2 cipher:aes-xts-plain64 key-size:64 pbkdf:argon2i"
    };

    check("/dev/sdc1", input, output);
}


BOOST_AUTO_TEST_CASE(parse3_paes)
{
    vector<string> input = {
	"LUKS header information",
	"Version:       	2",
	"Epoch:         	4",
	"Metadata area: 	12288 bytes",
	"UUID:          	22ff3407-ae5d-4bc6-b0cf-462b75e0b6a0",
	"Label:         	(no label)",
	"Subsystem:     	(no subsystem)",
	"Flags:       	(no flags)",
	"",
	"Data segments:",
	"  0: crypt",
	"	offset: 4194304 [bytes]",
	"	length: (whole device)",
	"	cipher: paes-xts-plain64",
	"	sector: 4096 [bytes]",
	"",
	"Keyslots:",
	"  0: luks2",
	"	Key:        1024 bits",
	"	Priority:   normal",
	"	Cipher:     aes-xts-plain64",
	"	PBKDF:      argon2i",
	"	Time cost:  4",
	"	Memory:     418482",
	"	Threads:    2",
	"	Salt:       d0 c0 97 e1 81 54 10 cd 00 42 01 89 b1 13 2b 36 ",
	"	            40 bc 92 c0 75 2a 5d cd 47 38 d6 8f cc 1d ec 61 ",
	"	AF stripes: 4000",
	"	Area offset:32768 [bytes]",
	"	Area length:512000 [bytes]",
	"	Digest ID:  0",
	"Tokens:",
	"  0: paes-verification-pattern",
	"Digests:",
	"  0: pbkdf2",
	"	Hash:       sha256",
	"	Iterations: 34204",
	"	Salt:       34 da 2c b0 59 73 a0 db b2 98 15 14 68 de 0a 13 ",
	"	            f0 cc e7 ef b6 9a 56 39 53 5e 55 88 c7 e3 f4 17 ",
	"	Digest:     b2 72 ac 7d 9f 7b 11 77 0b c6 2e 09 b9 28 95 8e ",
	"	            6e ed 93 91 00 5d c7 0b 68 3c 06 44 2f 7d 35 72 "
    };

    vector<string> output = {
	"name:/dev/dasdb1 uuid:22ff3407-ae5d-4bc6-b0cf-462b75e0b6a0 encryption-type:luks2 cipher:paes-xts-plain64 key-size:128 pbkdf:argon2i"
    };

    check("/dev/dasdb1", input, output);
}


BOOST_AUTO_TEST_CASE(parse4_two_keyslots)
{
    vector<string> input = {
	"LUKS header information",
	"Version:       	2",
	"Epoch:         	4",
	"Metadata area: 	16384 [bytes]",
	"Keyslots area: 	16744448 [bytes]",
	"UUID:          	30c4e059-7c30-4913-9c89-2d18bb818c87",
	"Label:         	(no label)",
	"Subsystem:     	(no subsystem)",
	"Flags:       	(no flags)",
	"",
	"Data segments:",
	"  0: crypt",
	"	offset: 16777216 [bytes]",
	"	length: (whole device)",
	"	cipher: aes-xts-plain64",
	"	sector: 4096 [bytes]",
	"",
	"Keyslots:",
	"  0: luks2",
	"	Key:        512 bits",
	"	Priority:   normal",
	"	Cipher:     aes-xts-plain64",
	"	Cipher key: 512 bits",
	"	PBKDF:      argon2id",
	"	Time cost:  10",
	"	Memory:     1048576",
	"	Threads:    4",
	"	Salt:       df 02 b0 b5 07 32 7c 21 ec 78 23 e8 94 06 fd fe ",
	"	            3f 38 23 03 97 dd 17 fd 1c b9 55 06 59 a5 bf a2 ",
	"	AF stripes: 4000",
	"	AF hash:    sha256",
	"	Area offset:32768 [bytes]",
	"	Area length:258048 [bytes]",
	"	Digest ID:  0",
	"  1: luks2",
	"	Key:        512 bits",
	"	Priority:   normal",
	"	Cipher:     aes-xts-plain64",
	"	Cipher key: 256 bits",
	"	PBKDF:      argon2i",
	"	Time cost:  9",
	"	Memory:     1048576",
	"	Threads:    4",
	"	Salt:       f7 e6 6a be 6a 05 38 57 cb 6a 1f 91 00 b9 e4 ab ",
	"	            bd dd 69 e7 2f c3 97 ee 99 3d a4 ba e8 6c d0 a8 ",
	"	AF stripes: 4000",
	"	AF hash:    sha256",
	"	Area offset:290816 [bytes]",
	"	Area length:258048 [bytes]",
	"	Digest ID:  0",
	"Tokens:",
	"Digests:",
	"  0: pbkdf2",
	"	Hash:       sha256",
	"	Iterations: 136818",
	"	Salt:       f0 d5 dd a7 ba 83 53 77 2f 62 89 ad 9b 87 03 46 ",
	"	            c7 c1 84 c9 bb 83 06 d0 7a c2 d5 fb 6b 6f 42 89 ",
	"	Digest:     5b da 8b c4 58 69 29 61 aa 77 1a 48 17 99 ef 6e ",
	"	            37 ba e2 64 35 c4 61 e0 27 38 98 61 0a fb 13 e1 "
    };

    vector<string> output = {
	"name:/dev/ram0p1 uuid:30c4e059-7c30-4913-9c89-2d18bb818c87 encryption-type:luks2 cipher:aes-xts-plain64 key-size:64 pbkdf:argon2id"
    };

    check("/dev/ram0p1", input, output);
}


BOOST_AUTO_TEST_CASE(parse5_aead)
{
    vector<string> input = {
	"LUKS header information",
	"Version:        2",
	"Epoch:          3",
	"Metadata area:  16384 [bytes]",
	"Keyslots area:  16744448 [bytes]",
	"UUID:           dfcefa36-2548-45b7-98f4-700bd80fa67a",
	"Label:          (no label)",
	"Subsystem:      (no subsystem)",
	"Flags:          (no flags)",
	"",
	"Data segments:",
	"  0: crypt",
	"        offset: 16777216 [bytes]",
	"        length: (whole device)",
	"        cipher: aegis128-random",
	"        sector: 512 [bytes]",
	"        integrity: aead",
	"",
	"Keyslots:",
	"  0: luks2",
	"        Key:        128 bits",
	"        Priority:   normal",
	"        Cipher:     aes-xts-plain64",
	"        Cipher key: 512 bits",
	"        PBKDF:      argon2id",
	"        Time cost:  4",
	"        Memory:     635984",
	"        Threads:    1",
	"        Salt:       cc 68 5d 98 ba 76 59 73 04 43 0d e1 5c 7a 3d 1b ",
	"                    4a 88 32 63 b4 ac 37 b3 9f 5a f0 d4 f0 92 ab 6f ",
	"        AF stripes: 4000",
	"        AF hash:    sha256",
	"        Area offset:32768 [bytes]",
	"        Area length:65536 [bytes]",
	"        Digest ID:  0",
	"Tokens:",
	"Digests:",
	"  0: pbkdf2",
	"        Hash:       sha256",
	"        Iterations: 154202",
	"        Salt:       bd 4f e2 58 e5 28 32 5e e7 d9 4e 20 fb 09 06 9c ",
	"                    99 a8 e0 4f f1 6d 52 4f b3 7a b2 37 c4 d0 3b c4 ",
	"        Digest:     31 6c a5 44 31 b2 75 39 05 c3 33 da ec 2f ad 5b ",
	"                    1e e4 8d b0 71 63 69 fc 76 af 48 ca 31 46 62 ab "
    };

    vector<string> output = {
	"name:/dev/sdc1 uuid:dfcefa36-2548-45b7-98f4-700bd80fa67a encryption-type:luks2 cipher:aegis128-random key-size:16 pbkdf:argon2id integrity:aead"
    };

    check("/dev/sdc1", input, output);
}
