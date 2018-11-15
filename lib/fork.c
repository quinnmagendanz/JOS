// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	///////////////////////MAGENDANZ///////////////////////////
	void *va = ROUNDDOWN(addr, PGSIZE);
	uint32_t perm = PTE_U | PTE_P | PTE_COW;
	if (!(err & FEC_WR) || (uvpt[PGNUM(va)] & perm) != perm)
		panic("pgfault not from write on copy-on-write page: err: %e, perms: %x", err, uvpt[PGNUM(va)]);
	//////////////////////////////////////////////////////////

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	/////////////////////////MAGENDANZ///////////////////////////
	// Allocate a new page, map it at a temporary location (PFTEMP)
	if ((r = sys_page_alloc(0, (void *) PFTEMP, PTE_P|PTE_U|PTE_W)) < 0) {
		panic("sys_page_alloc: %e", r);
	}
	// copy the data from the old page to the new page
	memcpy((void *) PFTEMP, va, PGSIZE);
	//then move the new page to the old page's address.
	if ((r = sys_page_map(0, (void *) PFTEMP, 0, va, PTE_P|PTE_U|PTE_W)) < 0) {
		panic("sys_page_map: %e", r);
	}
	if ((r = sys_page_unmap(0, (void *) PFTEMP)) < 0) {
		panic("sys_page_unmap: %e", r);
	}
	////////////////////////////////////////////////////////////
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{

	////////////////////MAGENDANZ///////////////////////
	void *addr = (void *) (pn * PGSIZE);
	uint32_t perm = PTE_U | PTE_P;
	int r;

	// If the page is sharable, copy the mapping directly.
	if (uvpt[pn] & PTE_SHARE) {
		if ((r = sys_page_map(0, addr, envid, addr, uvpt[pn] & PTE_SYSCALL)) < 0) {
			panic("sys_page_map: %e", r);
		}
		return 0;
	}
    
	// If the page is writable or copy-on-write, the new mapping must be created copy-on-write
	if (uvpt[pn] & (PTE_W | PTE_COW)) {
		perm |= PTE_COW;
	}
	
	if ((r = sys_page_map(0, addr, envid, addr, perm)) < 0) {
		panic("sys_page_map: %e", r);
	}
	// our mapping must be marked copy-on-write as well
	if ((r = sys_page_map(0, addr, 0, addr, perm)) < 0) {
		panic("sys_page_map: %e", r);
	}
	/////////////////////////////////////////////////////
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	int r;

	/////////////////MAGENDANZ/////////////////////
	set_pgfault_handler(&pgfault);

	envid_t new_envid = sys_exofork();
	if (new_envid < 0) 
		// failure 
		return new_envid;
	if (new_envid == 0) {
		// I am child.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	
	// I am parent.
	for (void* addr = 0; addr <  (void*)(UXSTACKTOP - PGSIZE); addr += PGSIZE) {	
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)) {
			duppage(new_envid, PGNUM(addr));
		}
	}

	if ((r = sys_page_alloc(new_envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U|PTE_W|PTE_P)) < 0) {
		panic("sys_page_alloc: %e", r);
	}	
	if ((r = sys_env_set_pgfault_upcall(new_envid, thisenv->env_pgfault_upcall)) < 0) {
		panic("sys_env_set_pgfault_upcall: %e", r);
	}	
	if ((r = sys_env_set_status(new_envid, ENV_RUNNABLE)) < 0) {
		panic("sys_env_set_status: %e", r);
	}

	return new_envid;
	/////////////////////////////////////////////////
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
