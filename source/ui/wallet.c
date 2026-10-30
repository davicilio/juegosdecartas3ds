#include "ui/wallet.h"

#include <3ds.h>

#include "ui/mii.h"

int wallet_chips(void)
{
	return profile_get()->chips;
}

int wallet_take(int want)
{
	int n = profile_take_chips(profile_get(), want);
	if (n > 0)
		profile_store();
	return n;
}

void wallet_give(int n)
{
	if (n <= 0)
		return;
	profile_give_chips(profile_get(), n);
	profile_store();
}

/* Las monedas de juego están en los datos compartidos del sistema
 * 0xF000000B, en gamecoin.dat: el total es un u16 en la posición 4. */
#define COIN_OFFSET 4
#define COIN_MAX    300

static bool open_coins(FS_Archive* archive, Handle* file, u32 flags)
{
	static const u32 extdata[3] = { MEDIATYPE_NAND, 0xF000000B, 0x00048000 };
	FS_Path apath = { PATH_BINARY, sizeof extdata, extdata };
	if (R_FAILED(FSUSER_OpenArchive(archive, ARCHIVE_SHARED_EXTDATA, apath)))
		return false;
	if (R_FAILED(FSUSER_OpenFile(file, *archive, fsMakePath(PATH_ASCII, "/gamecoin.dat"), flags, 0))) {
		FSUSER_CloseArchive(*archive);
		return false;
	}
	return true;
}

int playcoins_get(void)
{
	FS_Archive archive;
	Handle file;
	if (!open_coins(&archive, &file, FS_OPEN_READ))
		return -1;
	u16 coins = 0;
	u32 read = 0;
	Result r = FSFILE_Read(file, &read, COIN_OFFSET, &coins, sizeof coins);
	FSFILE_Close(file);
	FSUSER_CloseArchive(archive);
	if (R_FAILED(r) || read != sizeof coins)
		return -1;
	return coins > COIN_MAX ? COIN_MAX : coins;
}

bool playcoins_spend(int n)
{
	if (n <= 0)
		return false;
	FS_Archive archive;
	Handle file;
	if (!open_coins(&archive, &file, FS_OPEN_READ | FS_OPEN_WRITE))
		return false;
	u16 coins = 0;
	u32 done = 0;
	bool ok = R_SUCCEEDED(FSFILE_Read(file, &done, COIN_OFFSET, &coins, sizeof coins)) && done == sizeof coins &&
		coins >= n;
	if (ok) {
		coins -= n;
		ok = R_SUCCEEDED(FSFILE_Write(file, &done, COIN_OFFSET, &coins, sizeof coins, FS_WRITE_FLUSH)) &&
			done == sizeof coins;
	}
	FSFILE_Close(file);
	FSUSER_CloseArchive(archive);
	return ok;
}
