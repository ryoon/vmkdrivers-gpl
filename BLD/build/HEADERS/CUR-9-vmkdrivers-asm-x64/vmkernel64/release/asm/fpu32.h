#ifndef _FPU32_H
#define _FPU32_H 1

struct _fpstate_ia32;

int restore_i387_ia32(struct task_struct *tsk, struct _fpstate_ia32 __user *buf, int fsave);
int save_i387_ia32(struct task_struct *tsk, struct _fpstate_ia32 __user *buf, 
		   struct pt_regs *regs, int fsave);

int get_fpregs32(struct user_i387_ia32_struct *, struct task_struct *);
int set_fpregs32(struct task_struct *, const struct user_i387_ia32_struct *);

#endif
