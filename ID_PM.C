//
//	ID_PM.C
//	Id Engine's Page Manager v1.0
//	Primary coder: Jason Blochowiak
//

#include "ID_HEADS.H"
#pragma hdrstop

//	Main Mem specific variables
	boolean			MainPresent;
	memptr			MainMemPages[PMMaxMainMem];
	PMBlockAttr		MainMemUsed[PMMaxMainMem];
	int				MainPagesAvail;

//	EMS specific variables
	boolean			EMSPresent;
	word			EMSAvail,EMSPagesAvail,EMSHandle,
					EMSPageFrame,EMSPhysicalPage;
	EMSListStruct	EMSList[EMSFrameCount];

//	XMS specific variables
	boolean			XMSPresent;
	word			XMSAvail,XMSPagesAvail,XMSHandle;
	longword		XMSDriver;
	int				XMSProtectPage = -1;

//	File specific variables
	char			PageFileName[13] = {"VSWAP."};
	int				PageFile = -1;
	word			ChunksInFile;
	word			PMSpriteStart,PMSoundStart;

//	General usage variables
	int				PMStarted,
					PMPanicMode,
					PMThrashing;
	word			XMSPagesUsed,
					EMSPagesUsed,
					MainPagesUsed,
					PMNumBlocks;
	long			PMFrameCount;
	PageListStruct	*PMPages,
					*PMSegPages;

	// PORT
	//static	char		*ParmStrings[] = {"nomain","noems","noxms",nil};

/////////////////////////////////////////////////////////////////////////////
//
//	EMS Management code
//
/////////////////////////////////////////////////////////////////////////////

//
//	PML_MapEMS() - Maps a logical page to a physical page
//
void
PML_MapEMS(word logical,word physical)
{
	//_AL = physical;
	//_BX = logical;
	//_DX = EMSHandle;
	//_AH = EMS_MAPPAGE;
asm	int	EMS_INT

	//if (_AH)
	//	Quit("PML_MapEMS: Page mapping failed");
}

//
//	PML_StartupEMS() - Sets up EMS for Page Mgr's use
//		Checks to see if EMS driver is present
//      Verifies that EMS hardware is present
//		Make sure that EMS version is 3.2 or later
//		If there's more than our minimum (2 pages) available, allocate it (up
//			to the maximum we need)
//

	char	EMMDriverName[9] = "EMMXXXX0";

boolean
PML_StartupEMS(void)
{
	int		i;
	long	size;

	EMSPresent = False;			// Assume that we'll fail
	EMSAvail = 0;

	//_DX = (word)EMMDriverName;
	//_AX = 0x3d00;
	//geninterrupt(0x21);			// try to open EMMXXXX0 device
asm	jnc	gothandle
	goto error;

gothandle:
	//_BX = _AX;
	//_AX = 0x4400;
	//geninterrupt(0x21);			// get device info
asm	jnc	gotinfo;
	goto error;

gotinfo:
asm	and	dx,0x80
	//if (!_DX)
	//	goto error;

	//_AX = 0x4407;
	//geninterrupt(0x21);			// get status
asm	jc	error
	//if (!_AL)
	//	goto error;

	//_AH = 0x3e;
	//geninterrupt(0x21);			// close handle

	//_AH = EMS_STATUS;
	//geninterrupt(EMS_INT);
	//if (_AH)
	//	goto error;				// make sure EMS hardware is present

	//_AH = EMS_VERSION;
	//geninterrupt(EMS_INT);
	//if (_AH || (_AL < 0x32))	// only work on EMS 3.2 or greater (silly, but...)
	//	goto error;

	//_AH = EMS_GETFRAME;
	//geninterrupt(EMS_INT);
	//if (_AH)
	//	goto error;				// find the page frame address
	//EMSPageFrame = _BX;

	//_AH = EMS_GETPAGES;
	//geninterrupt(EMS_INT);
	//if (_AH)
	//	goto error;
	//if (_BX < 2)
	//	goto error;         	// Require at least 2 pages (32k)
	//EMSAvail = _BX;

	// Don't hog all available EMS
	size = EMSAvail * (long)EMSPageSize;
	if (size - (EMSPageSize * 2) > (ChunksInFile * (long)PMPageSize))
	{
		size = (ChunksInFile * (long)PMPageSize) + EMSPageSize;
		EMSAvail = size / EMSPageSize;
	}

	//_AH = EMS_ALLOCPAGES;
	//_BX = EMSAvail;
	//geninterrupt(EMS_INT);
	//if (_AH)
	//	goto error;
	//EMSHandle = _DX;

	mminfo.EMSmem += EMSAvail * (long)EMSPageSize;

	// Initialize EMS mapping cache
	for (i = 0;i < EMSFrameCount;i++)
		EMSList[i].baseEMSPage = -1;

	EMSPresent = True;			// We have EMS

error:
	return(EMSPresent);
}

//
//	PML_ShutdownEMS() - If EMS was used, deallocate it
//
void
PML_ShutdownEMS(void)
{
	if (EMSPresent)
	{
	asm	mov	ah,EMS_FREEPAGES
	asm	mov	dx,word ptr [EMSHandle]
	asm	int	EMS_INT
		/*if (_AH)
			Quit ("PML_ShutdownEMS: Error freeing EMS");*/
	}
}

/////////////////////////////////////////////////////////////////////////////
//
//	XMS Management code
//
/////////////////////////////////////////////////////////////////////////////

//
//	PML_StartupXMS() - Starts up XMS for the Page Mgr's use
//		Checks for presence of an XMS driver
//		Makes sure that there's at least a page of XMS available
//		Allocates any remaining XMS (rounded down to the nearest page size)
//
boolean
PML_StartupXMS(void)
{
	XMSPresent = False;					// Assume failure
	XMSAvail = 0;

asm	mov	ax,0x4300
asm	int	XMS_INT         				// Check for presence of XMS driver
	//if (_AL != 0x80)
	//	goto error;


asm	mov	ax,0x4310
asm	int	XMS_INT							// Get address of XMS driver
asm	mov	[WORD PTR XMSDriver],bx
asm	mov	[WORD PTR XMSDriver+2],es		// function pointer to XMS driver

	//XMS_CALL(XMS_QUERYFREE);			// Find out how much XMS is available
	//XMSAvail = _AX;
	//if (!_AX)				// AJR: bugfix 10/8/92
	//	goto error;

	XMSAvail &= ~(PMPageSizeKB - 1);	// Round off to nearest page size
	if (XMSAvail < (PMPageSizeKB * 2))	// Need at least 2 pages
		goto error;

	//_DX = XMSAvail;
	//XMS_CALL(XMS_ALLOC);				// And do the allocation
	//XMSHandle = _DX;

	//if (!_AX)				// AJR: bugfix 10/8/92
	//{
	//	XMSAvail = 0;
	//	goto error;
	//}

	mminfo.XMSmem += XMSAvail * 1024;

	XMSPresent = True;
error:
	return(XMSPresent);
}

//
//	PML_XMSCopy() - Copies a main/EMS page to or from XMS
//		Will round an odd-length request up to the next even value
//
void
PML_XMSCopy(boolean toxms,byte *addr,word xmspage,word length)
{
	longword	xoffset;
	struct
	{
		longword	length;
		word		source_handle;
		longword	source_offset;
		word		target_handle;
		longword	target_offset;
	} copy;

	if (!addr)
		Quit("PML_XMSCopy: zero address");

	xoffset = (longword)xmspage * PMPageSize;

	copy.length = (length + 1) & ~1;
	copy.source_handle = toxms? 0 : XMSHandle;
	copy.source_offset = toxms? (long)addr : xoffset;
	copy.target_handle = toxms? XMSHandle : 0;
	copy.target_offset = toxms? xoffset : (long)addr;

asm	push si
//	_SI = (word)&copy;
//	XMS_CALL(XMS_MOVE);
//asm	pop	si
//	if (!_AX)
//		Quit("PML_XMSCopy: Error on copy");
}

#if 1
#define	PML_CopyToXMS(s,t,l)	PML_XMSCopy(True,(s),(t),(l))
#define	PML_CopyFromXMS(t,s,l)	PML_XMSCopy(False,(t),(s),(l))
#else
//
//	PML_CopyToXMS() - Copies the specified number of bytes from the real mode
//		segment address to the specified XMS page
//
void
PML_CopyToXMS(byte *source,int targetpage,word length)
{
	PML_XMSCopy(True,source,targetpage,length);
}

//
//	PML_CopyFromXMS() - Copies the specified number of bytes from an XMS
//		page to the specified real mode address
//
void
PML_CopyFromXMS(byte *target,int sourcepage,word length)
{
	PML_XMSCopy(False,target,sourcepage,length);
}
#endif

//
//	PML_ShutdownXMS()
//
void
PML_ShutdownXMS(void)
{
	if (XMSPresent)
	{
		/*_DX = XMSHandle;
		XMS_CALL(XMS_FREE);
		if (_BL)
			Quit("PML_ShutdownXMS: Error freeing XMS");*/
	}
}

/////////////////////////////////////////////////////////////////////////////
//
//	Main memory code
//
/////////////////////////////////////////////////////////////////////////////

//
//	PM_SetMainMemPurge() - Sets the purge level for all allocated main memory
//		blocks. This shouldn't be called directly - the PM_LockMainMem() and
//		PM_UnlockMainMem() macros should be used instead.
//
void
PM_SetMainMemPurge(int level)
{
	int	i;

	for (i = 0;i < PMMaxMainMem;i++)
		if (MainMemPages[i])
			MM_SetPurge(&MainMemPages[i],level);
}

//
//	PM_CheckMainMem() - If something besides the Page Mgr makes requests of
//		the Memory Mgr, some of the Page Mgr's blocks may have been purged,
//		so this function runs through the block list and checks to see if
//		any of the blocks have been purged. If so, it marks the corresponding
//		page as purged & unlocked, then goes through the block list and
//		tries to reallocate any blocks that have been purged.
//	This routine now calls PM_LockMainMem() to make sure that any allocation
//		attempts made during the block reallocation sweep don't purge any
//		of the other blocks. Because PM_LockMainMem() is called,
//		PM_UnlockMainMem() needs to be called before any other part of the
//		program makes allocation requests of the Memory Mgr.
//
void
PM_CheckMainMem(void)
{
	boolean			allocfailed;
	int				i,n;
	memptr			*p;
	PMBlockAttr		*used;
	PageListStruct	*page;

	if (!MainPresent)
		return;

	for (i = 0,page = PMPages;i < ChunksInFile;i++,page++)
	{
		n = page->mainPage;
		if (n != -1)						// Is the page using main memory?
		{
			if (!MainMemPages[n])			// Yep, was the block purged?
			{
				page->mainPage = -1;		// Yes, mark page as purged & unlocked
				page->locked = pml_Unlocked;
			}
		}
	}

	// Prevent allocation attempts from purging any of our other blocks
	PM_LockMainMem();
	allocfailed = False;
	for (i = 0,p = MainMemPages,used = MainMemUsed;i < PMMaxMainMem;i++,p++,used++)
	{
		if (!*p)							// If the page got purged
		{
			if (*used & pmba_Allocated)		// If it was allocated
			{
				*used = (PMBlockAttr)(*used & ~pmba_Allocated);	// Mark as unallocated // PORT add cast
				MainPagesAvail--;			// and decrease available count
			}

			if (*used & pmba_Used)			// If it was used
			{
				*used = (PMBlockAttr)(*used & ~pmba_Used);		// Mark as unused // PORT add cast
				MainPagesUsed--;			// and decrease used count
			}

			if (!allocfailed)
			{
				MM_BombOnError(False);
				MM_GetPtr(p,PMPageSize);		// Try to reallocate
				if (mmerror)					// If it failed,
					allocfailed = True;			//  don't try any more allocations
				else							// If it worked,
				{
					*used = (PMBlockAttr)(*used | pmba_Allocated);	// Mark as allocated // PORT add cast
					MainPagesAvail++;			// and increase available count
				}
				MM_BombOnError(True);
			}
		}
	}
	if (mmerror)
		mmerror = False;
}

//
//	PML_StartupMainMem() - Allocates as much main memory as is possible for
//		the Page Mgr. The memory is allocated as non-purgeable, so if it's
//		necessary to make requests of the Memory Mgr, PM_UnlockMainMem()
//		needs to be called.
//
void
PML_StartupMainMem(void)
{
	int		i,n;
	memptr	*p;

	MainPagesAvail = 0;
	MM_BombOnError(False);
	for (i = 0,p = MainMemPages;i < PMMaxMainMem;i++,p++)
	{
		MM_GetPtr(p,PMPageSize);
		if (mmerror)
			break;

		MainPagesAvail++;
		MainMemUsed[i] = pmba_Allocated;
	}
	MM_BombOnError(True);
	if (mmerror)
		mmerror = False;
	if (MainPagesAvail < PMMinMainMem)
		Quit("PM_SetupMainMem: Not enough main memory");
	MainPresent = True;
}

//
//	PML_ShutdownMainMem() - Frees all of the main memory blocks used by the
//		Page Mgr.
//
void
PML_ShutdownMainMem(void)
{
	int		i;
	memptr	*p;

	// DEBUG - mark pages as unallocated & decrease page count as appropriate
	for (i = 0,p = MainMemPages;i < PMMaxMainMem;i++,p++)
		if (*p)
			MM_FreePtr(p);
}

/////////////////////////////////////////////////////////////////////////////
//
//	File management code
//
/////////////////////////////////////////////////////////////////////////////

//
//	PML_ReadFromFile() - Reads some data in from the page file
//
void
PML_ReadFromFile(byte *buf,long offset,word length)
{
	if (!buf)
		Quit("PML_ReadFromFile: Null pointer");
	if (!offset)
		Quit("PML_ReadFromFile: Zero offset");
	if (_lseek(PageFile,offset,SEEK_SET) != offset)
		Quit("PML_ReadFromFile: Seek failed");
	if (!_read(PageFile,buf,length))
		Quit("PML_ReadFromFile: Read failed");
}

//
//	PML_OpenPageFile() - Opens the page file and sets up the page info
//
void
PML_OpenPageFile(void)
{
	int				i;
	long			size;
	void			*buf;
	longword		*offsetptr;
	word			*lengthptr;
	PageListStruct	*page;

	PageFile = _open(PageFileName,O_RDONLY + O_BINARY);
	if (PageFile == -1)
		Quit("PML_OpenPageFile: Unable to open page file");

	// Read in header variables
	_read(PageFile,&ChunksInFile,sizeof(ChunksInFile));
	_read(PageFile,&PMSpriteStart,sizeof(PMSpriteStart));
	_read(PageFile,&PMSoundStart,sizeof(PMSoundStart));

	// Allocate and clear the page list
	PMNumBlocks = ChunksInFile;
	MM_GetPtr((memptr *)&PMSegPages,sizeof(PageListStruct) * PMNumBlocks);
	MM_SetLock((memptr *)&PMSegPages,True);
	PMPages = (PageListStruct *)PMSegPages;
	memset(PMPages,0,sizeof(PageListStruct) * PMNumBlocks);

	// Read in the chunk offsets
	size = sizeof(longword) * ChunksInFile;
	MM_GetPtr(&buf,size);
	if (!_read(PageFile,(byte *)buf,size))
		Quit("PML_OpenPageFile: Offset read failed");
	offsetptr = (longword *)buf;
	for (i = 0,page = PMPages;i < ChunksInFile;i++,page++)
		page->offset = *offsetptr++;
	MM_FreePtr(&buf);

	// Read in the chunk lengths
	size = sizeof(word) * ChunksInFile;
	MM_GetPtr(&buf,size);
	if (!_read(PageFile,(byte *)buf,size))
		Quit("PML_OpenPageFile: Length read failed");
	lengthptr = (word *)buf;
	for (i = 0,page = PMPages;i < ChunksInFile;i++,page++)
		page->length = *lengthptr++;
	MM_FreePtr(&buf);
}

//
//  PML_ClosePageFile() - Closes the page file
//
void
PML_ClosePageFile(void)
{
	if (PageFile != -1)
		close(PageFile);
	if (PMSegPages)
	{
		MM_SetLock((memptr *)&PMSegPages,False);
		MM_FreePtr((memptr *)&PMSegPages);
	}
}

/////////////////////////////////////////////////////////////////////////////
//
//	Allocation, etc., code
//
/////////////////////////////////////////////////////////////////////////////

//
//	PML_GetEMSAddress()
//
// 		Page is in EMS, so figure out which EMS physical page should be used
//  		to map our page in. If normal page, use EMS physical page 3, else
//  		use the physical page specified by the lock type
//
#if 1
#pragma argsused	// DEBUG - remove lock parameter
memptr
PML_GetEMSAddress(int page,PMLockType lock)
{
	int		i,emspage;
	word	emsoff,emsbase,offset;

	emsoff = page & (PMEMSSubPage - 1);
	emsbase = page - emsoff;

	emspage = -1;
	// See if this page is already mapped in
	for (i = 0;i < EMSFrameCount;i++)
	{
		if (EMSList[i].baseEMSPage == emsbase)
		{
			emspage = i;	// Yep - don't do a redundant remapping
			break;
		}
	}

	// If page isn't already mapped in, find LRU EMS frame, and use it
	if (emspage == -1)
	{
		longword last = ULONG_MAX;
		for (i = 0;i < EMSFrameCount;i++)
		{
			if (EMSList[i].lastHit < last)
			{
				emspage = i;
				last = EMSList[i].lastHit;
			}
		}

		EMSList[emspage].baseEMSPage = emsbase;
		PML_MapEMS(page / PMEMSSubPage,emspage);
	}

	if (emspage == -1)
		Quit("PML_GetEMSAddress: EMS find failed");

	EMSList[emspage].lastHit = PMFrameCount;
	offset = emspage * EMSPageSizeSeg;
	offset += emsoff * PMPageSizeSeg;
	return((memptr)(EMSPageFrame + offset));
}
#else
memptr
PML_GetEMSAddress(int page,PMLockType lock)
{
	word	emspage;

	emspage = (lock < pml_EMSLock)? 3 : (lock - pml_EMSLock);

	PML_MapEMS(page / PMEMSSubPage,emspage);

	return((memptr)(EMSPageFrame + (emspage * EMSPageSizeSeg)
			+ ((page & (PMEMSSubPage - 1)) * PMPageSizeSeg)));
}
#endif

//
//	PM_GetPageAddress() - Returns the address of a given page
//		Maps in EMS if necessary
//		Returns nil if block isn't cached into Main Memory or EMS
//
//
memptr
PM_GetPageAddress(int pagenum)
{
	PageListStruct	*page;

	page = &PMPages[pagenum];
	if (page->mainPage != -1)
		return(MainMemPages[page->mainPage]);
	else if (page->emsPage != -1)
		return(PML_GetEMSAddress(page->emsPage,page->locked));
	else
		return(nil);
}

//
//	PML_GiveLRUPage() - Returns the page # of the least recently used
//		present & unlocked main/EMS page (or main page if mainonly is True)
//
int
PML_GiveLRUPage(boolean mainonly)
{
	int				i,lru;
	long			last;
	PageListStruct	*page;

	for (i = 0,page = PMPages,lru = -1,last = LONG_MAX;i < ChunksInFile;i++,page++)
	{
		if
		(
			(page->lastHit < last)
		&&	((page->emsPage != -1) || (page->mainPage != -1))
		&& 	(page->locked == pml_Unlocked)
		&&	(!(mainonly && (page->mainPage == -1)))
		)
		{
			last = page->lastHit;
			lru = i;
		}
	}

	if (lru == -1)
		Quit("PML_GiveLRUPage: LRU Search failed");
	return(lru);
}

//
//	PML_GiveLRUXMSPage() - Returns the page # of the least recently used
//		(and present) XMS page.
//	This routine won't return the XMS page protected (by XMSProtectPage)
//
int
PML_GiveLRUXMSPage(void)
{
	int				i,lru;
	long			last;
	PageListStruct	*page;

	for (i = 0,page = PMPages,lru = -1,last = LONG_MAX;i < ChunksInFile;i++,page++)
	{
		if
		(
			(page->xmsPage != -1)
		&&	(page->lastHit < last)
		&&	(i != XMSProtectPage)
		)
		{
			last = page->lastHit;
			lru = i;
		}
	}
	return(lru);
}

//
//	PML_PutPageInXMS() - If page isn't in XMS, find LRU XMS page and replace
//		it with the main/EMS page
//
void
PML_PutPageInXMS(int pagenum)
{
	int				usexms;
	PageListStruct	*page;

	if (!XMSPresent)
		return;

	page = &PMPages[pagenum];
	if (page->xmsPage != -1)
		return;					// Already in XMS

	if (XMSPagesUsed < XMSPagesAvail)
		page->xmsPage = XMSPagesUsed++;
	else
	{
		usexms = PML_GiveLRUXMSPage();
		if (usexms == -1)
			Quit("PML_PutPageInXMS: No XMS LRU");
		page->xmsPage = PMPages[usexms].xmsPage;
		PMPages[usexms].xmsPage = -1;
	}
	PML_CopyToXMS((byte *)PM_GetPageAddress(pagenum),page->xmsPage,page->length); // PORT add (byte *) cast
}

//
//	PML_TransferPageSpace() - A page is being replaced, so give the new page
//		the old one's address space. Returns the address of the new page.
//
memptr
PML_TransferPageSpace(int orig,int New)
{
	memptr			addr;
	PageListStruct	*origpage,*newpage;

	if (orig == New)
		Quit("PML_TransferPageSpace: Identity replacement");

	origpage = &PMPages[orig];
	newpage = &PMPages[New];

	if (origpage->locked != pml_Unlocked)
		Quit("PML_TransferPageSpace: Killing locked page");

	if ((origpage->emsPage == -1) && (origpage->mainPage == -1))
		Quit("PML_TransferPageSpace: Reusing non-existent page");

	// Copy page that's about to be purged into XMS
	PML_PutPageInXMS(orig);

	// Get the address, and force EMS into a physical page if necessary
	addr = PM_GetPageAddress(orig);

	// Steal the address
	newpage->emsPage = origpage->emsPage;
	newpage->mainPage = origpage->mainPage;

	// Mark replaced page as purged
	origpage->mainPage = origpage->emsPage = -1;

	if (!addr)
		Quit("PML_TransferPageSpace: Zero replacement");

	return(addr);
}

//
//	PML_GetAPageBuffer() - A page buffer is needed. Either get it from the
//		main/EMS free pool, or use PML_GiveLRUPage() to find which page to
//		steal the buffer from. Returns a pointer to the page buffer, and
//		sets the fields inside the given page structure appropriately.
//		If mainonly is True, free EMS will be ignored, and only main pages
//		will be looked at by PML_GiveLRUPage().
//
byte *
PML_GetAPageBuffer(int pagenum,boolean mainonly)
{
	byte			*addr = (byte *)nil; // PORT add cast
	int				i,n;
	PMBlockAttr		*used;
	PageListStruct	*page;

	page = &PMPages[pagenum];
	if ((EMSPagesUsed < EMSPagesAvail) && !mainonly)
	{
		// There's remaining EMS - use it
		page->emsPage = EMSPagesUsed++;
		addr = (byte *)PML_GetEMSAddress(page->emsPage,page->locked); // PORT add cast
	}
	else if (MainPagesUsed < MainPagesAvail)
	{
		// There's remaining main memory - use it
		for (i = 0,n = -1,used = MainMemUsed;i < PMMaxMainMem;i++,used++)
		{
			if ((*used & pmba_Allocated) && !(*used & pmba_Used))
			{
				n = i;
				*used = (PMBlockAttr)(pmba_Used | (int)(*used)); // PORT add cast
				break;
			}
		}
		if (n == -1)
			Quit("PML_GetPageBuffer: MainPagesAvail lied");
		addr = (byte *)MainMemPages[n]; // PORT add cast
		if (!addr)
			Quit("PML_GetPageBuffer: Purged main block");
		page->mainPage = n;
		MainPagesUsed++;
	}
	else
		addr = (byte *)PML_TransferPageSpace(PML_GiveLRUPage(mainonly),pagenum); // PORT add cast

	if (!addr)
		Quit("PML_GetPageBuffer: Search failed");
	return(addr);
}

//
//	PML_GetPageFromXMS() - If page is in XMS, find LRU main/EMS page and
//		replace it with the page from XMS. If mainonly is True, will only
//		search for LRU main page.
//	XMSProtectPage is set to the page to be retrieved from XMS, so that if
//		the page from which we're stealing the main/EMS from isn't in XMS,
//		it won't copy over the page that we're trying to get from XMS.
//		(pages that are being purged are copied into XMS, if possible)
//
memptr
PML_GetPageFromXMS(int pagenum,boolean mainonly)
{
	byte			*checkaddr;
	memptr			addr = nil;
	PageListStruct	*page;

	page = &PMPages[pagenum];
	if (XMSPresent && (page->xmsPage != -1))
	{
		XMSProtectPage = pagenum;
		checkaddr = PML_GetAPageBuffer(pagenum,mainonly);
		// PORT
		/*if (FP_OFF(checkaddr))
			Quit("PML_GetPageFromXMS: Non segment pointer");
		addr = (memptr)FP_SEG(checkaddr);*/
		PML_CopyFromXMS((byte *)addr,page->xmsPage,page->length); // PORT add cast
		XMSProtectPage = -1;
	}

	return(addr);
}

//
//	PML_LoadPage() - A page is not in main/EMS memory, and it's not in XMS.
//		Load it into either main or EMS. If mainonly is True, the page will
//		only be loaded into main.
//
void
PML_LoadPage(int pagenum,boolean mainonly)
{
	byte			*addr;
	PageListStruct	*page;

	addr = PML_GetAPageBuffer(pagenum,mainonly);
	page = &PMPages[pagenum];
	PML_ReadFromFile(addr,page->offset,page->length);
}

//
//	PM_GetPage() - Returns the address of the page, loading it if necessary
//		First, check if in Main Memory or EMS
//		Then, check XMS
//		If not in XMS, load into Main Memory or EMS
//
#pragma warn -pia
memptr
PM_GetPage(int pagenum)
{
	memptr	result;

	if (pagenum >= ChunksInFile)
		Quit("PM_GetPage: Invalid page request");

#if 0	// for debugging
asm	mov	dx,STATUS_REGISTER_1
asm	in	al,dx
asm	mov	dx,ATR_INDEX
asm	mov	al,ATR_OVERSCAN
asm	out	dx,al
asm	mov	al,10	// bright green
asm	out	dx,al
#endif

	if (!(result = PM_GetPageAddress(pagenum)))
	{
		boolean mainonly = (boolean)(pagenum >= PMSoundStart);
if (!PMPages[pagenum].offset)	// JDC: sparse page
	Quit ("Tried to load a sparse page!");
		if (!(result = PML_GetPageFromXMS(pagenum,mainonly)))
		{
			if (PMPages[pagenum].lastHit == PMFrameCount)
				PMThrashing++;

			PML_LoadPage(pagenum,mainonly);
			result = PM_GetPageAddress(pagenum);
		}
	}
	PMPages[pagenum].lastHit = PMFrameCount;

#if 0	// for debugging
asm	mov	dx,STATUS_REGISTER_1
asm	in	al,dx
asm	mov	dx,ATR_INDEX
asm	mov	al,ATR_OVERSCAN
asm	out	dx,al
asm	mov	al,3	// blue
asm	out	dx,al
asm	mov	al,0x20	// normal
asm	out	dx,al
#endif

	return(result);
}
#pragma warn +pia

//
//	PM_SetPageLock() - Sets the lock type on a given page
//		pml_Unlocked: Normal, page can be purged
//		pml_Locked: Cannot be purged
//		pml_EMS?: Same as pml_Locked, but if in EMS, use the physical page
//					specified when returning the address. For sound stuff.
//
void
PM_SetPageLock(int pagenum,PMLockType lock)
{
	if (pagenum < PMSoundStart)
		Quit("PM_SetPageLock: Locking/unlocking non-sound page");

	PMPages[pagenum].locked = lock;
}

//
//	PM_Preload() - Loads as many pages as possible into all types of memory.
//		Calls the update function after each load, indicating the current
//		page, and the total pages that need to be loaded (for thermometer).
//
void
PM_Preload(boolean (*update)(word current,word total))
{
	int				i,j,
					page,oogypage;
	word			current,total,
					totalnonxms,totalxms,
					mainfree,maintotal,
					emsfree,emstotal,
					xmsfree,xmstotal;
	memptr			addr;
	PageListStruct	*p;

	mainfree = (MainPagesAvail - MainPagesUsed) + (EMSPagesAvail - EMSPagesUsed);
	xmsfree = (XMSPagesAvail - XMSPagesUsed);

	xmstotal = maintotal = 0;

	for (i = 0;i < ChunksInFile;i++)
	{
		if (!PMPages[i].offset)
			continue;			// sparse

		if ( PMPages[i].emsPage != -1 || PMPages[i].mainPage != -1 )
			continue;			// already in main mem

		if ( mainfree )
		{
			maintotal++;
			mainfree--;
		}
		else if ( xmsfree && (PMPages[i].xmsPage == -1) )
		{
			xmstotal++;
			xmsfree--;
		}
	}


	total = maintotal + xmstotal;

	if (!total)
		return;

	page = 0;
	current = 0;

//
// cache main/ems blocks
//
	while (maintotal)
	{
		while ( !PMPages[page].offset || PMPages[page].mainPage != -1
			||	PMPages[page].emsPage != -1 )
			page++;

		if (page >= ChunksInFile)
			Quit ("PM_Preload: Pages>=ChunksInFile");

		PM_GetPage(page);

		page++;
		current++;
		maintotal--;
		update(current,total);
	}

//
// load stuff to XMS
//
	if (xmstotal)
	{
		for (oogypage = 0 ; PMPages[oogypage].mainPage == -1 ; oogypage++)
		;
		addr = PM_GetPage(oogypage);
		if (!addr)
			Quit("PM_Preload: XMS buffer failed");

		while (xmstotal)
		{
			while ( !PMPages[page].offset || PMPages[page].xmsPage != -1 )
				page++;

			if (page >= ChunksInFile)
				Quit ("PM_Preload: Pages>=ChunksInFile");

			p = &PMPages[page];

			p->xmsPage = XMSPagesUsed++;
			if (XMSPagesUsed > XMSPagesAvail)
				Quit("PM_Preload: Exceeded XMS pages");
			if (p->length > PMPageSize)
				Quit("PM_Preload: Page too long");

			PML_ReadFromFile((byte *)addr,p->offset,p->length);
			PML_CopyToXMS((byte *)addr,p->xmsPage,p->length);

			page++;
			current++;
			xmstotal--;
			update(current,total);
		}

		p = &PMPages[oogypage];
		PML_ReadFromFile((byte *)addr,p->offset,p->length);
	}

	update(total,total);
}

/////////////////////////////////////////////////////////////////////////////
//
//	General code
//
/////////////////////////////////////////////////////////////////////////////

//
//	PM_NextFrame() - Increments the frame counter and adjusts the thrash
//		avoidence variables
//
//		If currently in panic mode (to avoid thrashing), check to see if the
//			appropriate number of frames have passed since the last time that
//			we would have thrashed. If so, take us out of panic mode.
//
//
void
PM_NextFrame(void)
{
	int	i;

	// Frame count overrun - kill the LRU hit entries & reset frame count
	if (++PMFrameCount >= LONG_MAX - 4)
	{
		for (i = 0;i < PMNumBlocks;i++)
			PMPages[i].lastHit = 0;
		PMFrameCount = 0;
	}

#if 0
	for (i = 0;i < PMSoundStart;i++)
	{
		if (PMPages[i].locked)
		{
			char buf[40];
			sprintf(buf,"PM_NextFrame: Page %d is locked",i);
			Quit(buf);
		}
	}
#endif

	if (PMPanicMode)
	{
		// DEBUG - set border color
		if ((!PMThrashing) && (!--PMPanicMode))
		{
			// DEBUG - reset border color
		}
	}
	if (PMThrashing >= PMThrashThreshold)
		PMPanicMode = (boolean)PMUnThrashThreshold;
	PMThrashing = False;
}

//
//	PM_Reset() - Sets up caching structures
//
void
PM_Reset(void)
{
	int				i;
	PageListStruct	*page;

	XMSPagesAvail = XMSAvail / PMPageSizeKB;

	EMSPagesAvail = EMSAvail * (EMSPageSizeKB / PMPageSizeKB);
	EMSPhysicalPage = 0;

	MainPagesUsed = EMSPagesUsed = XMSPagesUsed = 0;

	PMPanicMode = False;

	// Initialize page list
	for (i = 0,page = PMPages;i < PMNumBlocks;i++,page++)
	{
		page->mainPage = -1;
		page->emsPage = -1;
		page->xmsPage = -1;
		page->locked = (PMLockType)False;
	}
}

//
//	PM_Startup() - Start up the Page Mgr
//
void
PM_Startup(void)
{
	boolean	nomain,noems,noxms;
	int		i;

	if (PMStarted)
		return;

	nomain = noems = noxms = False;
	// PORT
	/*for (i = 1;i < _argc;i++)
	{
		switch (US_CheckParm(_argv[i],ParmStrings))
		{
		case 0:
			nomain = True;
			break;
		case 1:
			noems = True;
			break;
		case 2:
			noxms = True;
			break;
		}
	}*/

	PML_OpenPageFile();

	if (!noems)
		PML_StartupEMS();
	if (!noxms)
		PML_StartupXMS();

	if (nomain && !EMSPresent)
		Quit("PM_Startup: No main or EMS");
	else
		PML_StartupMainMem();

	PM_Reset();

	PMStarted = True;
}

//
//	PM_Shutdown() - Shut down the Page Mgr
//
void
PM_Shutdown(void)
{
	PML_ShutdownXMS();
	PML_ShutdownEMS();

	if (!PMStarted)
		return;

	PML_ClosePageFile();

	PML_ShutdownMainMem();
}
