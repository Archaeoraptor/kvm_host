// see https://www.kernel.org/doc/Documentation/virtual/kvm/api.txt

#include <asm/kvm.h>
#include <errno.h>
#include <stdio.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

int main(int argc, char *argv[])
{
    int kvm_fd;
    int vm_fd;
    int vcpu_fd;
    void *mem;

    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <guest-image>\n", argv[0]);
        return 1;
    }

    // kvm layer
    if ((kvm_fd = open("/dev/kvm", O_RDWR) < 0)) {
        fprintf(stderr, "failed to open /dev/kvm: %d\n", errno);
        return 1;
    }
    int version = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    printf("KMV version: %d\n", version);

    // create VM host
    if ((vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0) < 0)) {
        fprintf(stderr, "USAGE: %s <guest-image>\n", errno);
        return 1;
    }

    // create vm memory

    int RAM_SIZE = 0x10000;

    if ((mem = mmap(NULL,
                    RAM_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                    -1,
                    0))
        == NULL) {
        fprintf(stderr, "mmap failed: %d\n", errno);
        return 1;
    }

    // init vm memory
    struct kvm_userspace_memory_region region;
    memset(&region, 0, sizeof(region));
    region.slot = 0;
    region.guest_phys_addr = 0;
    region.memory_size = 1 << 30;
    region.userspace_addr = (uintptr_t) mem;
    if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
        fprintf(stderr, "ioctl KVM_SET_USER_MEMORY_REGION failed: %d\n", errno);
        return 1;
    }

    // load vm image and init memory
    int img_fd = open(argv[1], O_RDONLY);
    if (img_fd < 0) {
        fprintf(stderr, "cannot open binary guest file");
        return 1;
    }
    char *p = (char *) mem;

    for (;;) {
        int r = read(img_fd, p, 4096);
        if (r <= 0) {
            break;
        }
        p += r;
    }
    close(img_fd);

    // create vcpu
    if ((vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0)) < 0) {
        fprintf(stderr, "can not create vcpu: %d\n", errno);
        return 1;
    }

    // KVM_RUM is a memory area which stores run state
    int kvm_run_mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (kvm_run_mmap_size < 0) {
        fprintf(stderr, "ioctl KVM_GET_VCPU_MMAP_SIZE: %d\n", errno);
        return 1;
    }

    struct kvm_run *run = (struct kvm_run *)
        mmap(NULL, kvm_run_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);
    if (run == NULL) {
        fprintf(stderr, "mmap kvm_run");
    }
    if (run == NULL) {
        fprintf(stderr, "mmap kvm_run: %d\n", errno);
        return 1;
    }

    // registers
    struct kvm_regs regs;
    // special registers
    struct kvm_sregs sregs;

    if (ioctl(vcpu_fd, KVM_GET_SREGS, &(sregs)) < 0) {
        perror("can not get sregs\n");
        exit(1);
    }

#define CODE_START 0x0000
    // 16 bit mode
    sregs.cs.selector = CODE_START; // code segment
    sregs.cs.base = CODE_START * 16;
    sregs.ss.selector = CODE_START; // stack segment
    sregs.ss.base = CODE_START * 16;
    sregs.ds.selector =
}