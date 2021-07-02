TOOLPATH = ../z_tools/
INCPATH  = ../z_tools/haribote/

MAKE     = $(TOOLPATH)make.exe -r
EDIMG    = $(TOOLPATH)edimg.exe
IMGTOL   = $(TOOLPATH)imgtol.com
COPY     = copy
DEL      = del

default :
	$(MAKE) haribote.img

haribote.img : haribote/ipl20.bin haribote/haribote.sys Makefile \
		a/a.hrb hello3/hello3.hrb hello4/hello4.hrb hello5/hello5.hrb \
		winhello/winhello.hrb star/star.hrb line/line.hrb walk/walk.hrb \
		noodle/noodle.hrb beepdown/beepdown.hrb color/color.hrb sosu/sosu.hrb \
		typeipl/typeipl.hrb type/type.hrb  iroha/iroha.hrb
	$(EDIMG)   imgin:../z_tools/fdimg0at.tek \
		wbinimg src:haribote/ipl20.bin len:512 from:0 to:0 \
		copy from:haribote/haribote.sys to:@: \
		copy from:haribote/ipl20.nas to:@: \
		copy from:make.bat to:@: \
		copy from:a/a.hrb to:@: \
		copy from:hello3/hello3.hrb to:@: \
		copy from:hello4/hello4.hrb to:@: \
		copy from:hello5/hello5.hrb to:@: \
		copy from:winhello/winhello.hrb to:@: \
		copy from:star/star.hrb to:@: \
		copy from:line/line.hrb to:@: \
		copy from:walk/walk.hrb to:@: \
		copy from:noodle/noodle.hrb to:@: \
		copy from:beepdown/beepdown.hrb to:@: \
		copy from:color/color.hrb to:@: \
		copy from:sosu/sosu.hrb to:@: \
		copy from:typeipl/typeipl.hrb to:@: \
		copy from:type/type.hrb to:@: \
		copy from:iroha/iroha.hrb to:@: \
		copy from:nihongo/nihongo.fnt to:@: \
		imgout:haribote.img

# make -C change dir and do make
run :
	$(MAKE) haribote.img
	$(COPY) haribote.img ..\z_tools\qemu\fdimage0.bin
	$(MAKE) -C ../z_tools/qemu

install :
	$(MAKE) haribote.img
	$(IMGTOL) w a: haribote.img

full :
	$(MAKE) -C haribote
	$(MAKE) -C apilib
	$(MAKE) -C a
	$(MAKE) -C hello3
	$(MAKE) -C hello4
	$(MAKE) -C hello5
	$(MAKE) -C winhello
	$(MAKE) -C star
	$(MAKE) -C line
	$(MAKE) -C walk
	$(MAKE) -C noodle
	$(MAKE) -C beepdown
	$(MAKE) -C color
	$(MAKE) -C sosu
	$(MAKE) -C typeipl
	$(MAKE) -C type
	$(MAKE) -C iroha
	$(MAKE) haribote.img

run_full :
	$(MAKE) full
	$(COPY) haribote.img ..\z_tools\qemu\fdimage0.bin
	$(MAKE) -C ../z_tools/qemu

install_full :
	$(MAKE) full
	$(IMGTOL) w a: haribote.img

run_os :
	$(MAKE) -C haribote
	$(MAKE) run

clean :

src_only :
	$(MAKE) clean
	-$(DEL) haribote.img

clean_full :
	$(MAKE) -C haribote		clean
	$(MAKE) -C apilib		clean
	$(MAKE) -C a			clean
	$(MAKE) -C hello3		clean
	$(MAKE) -C hello4		clean
	$(MAKE) -C hello5		clean
	$(MAKE) -C winhello		clean
	$(MAKE) -C star 		clean
	$(MAKE) -C line 		clean
	$(MAKE) -C walk			clean
	$(MAKE) -C noodle		clean
	$(MAKE) -C beepdown		clean
	$(MAKE) -C color		clean
	$(MAKE) -C sosu			clean
	$(MAKE) -C typeipl		clean
	$(MAKE) -C type			clean
	$(MAKE) -C iroha		clean

src_only_full :
	$(MAKE) -C haribote		src_only
	$(MAKE) -C apilib		src_only
	$(MAKE) -C a			src_only
	$(MAKE) -C hello3		src_only
	$(MAKE) -C hello4		src_only
	$(MAKE) -C hello5		src_only
	$(MAKE) -C winhello		src_only
	$(MAKE) -C star 		src_only
	$(MAKE) -C line 		src_only
	$(MAKE) -C walk			src_only
	$(MAKE) -C noodle		src_only
	$(MAKE) -C beepdown		src_only
	$(MAKE) -C color		src_only
	$(MAKE) -C sosu			src_only
	$(MAKE) -C typeipl		src_only
	$(MAKE) -C type			src_only
	$(MAKE) -C iroha		src_only
	-$(DEL) haribote.img

refresh :
	$(MAKE) full
	$(MAKE) clean_full
	-$(DEL) haribote.img
