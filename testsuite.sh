#!/bin/bash

#algorithm for test suite

#run encoding on each of the yuv files
#2,3,5-45
#save psnr info
#rename mbinfo each time
#replace temp_flo files each file

declare -a ARRAY_SEQS_TYPES=(final)
declare -a ARRAY_SEQS=(alley_1 ambush_5 bamboo_2 cave_4  market_5 mountain_1 shaman_3 temple_2 )
#declare -a ARRAY_LIVE_SEQ=(ducks blueSky crowd oldTown)
declare -a ARRAY_QP=(2 3 5 7 10 15 20 25 30)

for TYPE in "${ARRAY_SEQS_TYPES[@]}"
do
	for SEQ in "${ARRAY_SEQS[@]}"
	do
		#copy files to temp folder
		cp -r /home/dj/data/flow/sintel/final/${SEQ}/. /home/dj/temp_flo

		for Q in "${ARRAY_QP[@]}"
		do
			#run ffmpeg
			#ffmpeg -s:v 1024x436 -r 25 -i /home/dj/data/raw/${TYPE}/${SEQ}.yuv -c:v mpeg4 -q ${Q} -pix_fmt yuv420p -g 100 -bf 1 -vframes 200 -psnr /home/dj/data/results/${TYPE}_${SEQ}_${Q}_b1_flo.mp4 -y
			ffmpeg -s:v 1024x436 -r 25 -i /home/dj/data/raw/${TYPE}/${SEQ}.yuv -c:v libvpx-vp9 -crf $Q -b:v 0 -psnr -bf 1 /home/dj/data/results/${TYPE}_${SEQ}_${Q}_b1_block.webm
		#rename MBinfo file
		mv /home/dj/temp_flo/MBInfo.txt /home/dj/temp_flo/${TYPE}_${SEQ}_${Q}_b1_flo.csv
		done
		#remove flo files
		rm -r /home/dj/temp_flo/*.flo 
	done
done

