#/* makefile -- 
# * Copyright (c) 2016--, ZhongWeiTao 
# * All rights reserved.
# */

CC = g++
#CPPFLAGS = -g -O3 -Wall -std=c++11 -lprotobuf -ltcmalloc
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DSDS_TEST_MAIN
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf  -DCODEC_TEST_MAIN
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DCODEC_TEST_MAIN
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DSDS_TEST_MAIN
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DCLIENT_UNIT_TEST_MAIN 
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DSKILLS_PACKAGE_TEST_MAIN
CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DCCSERVER_PP_CLIENT
#CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc 
objects = zmalloc.o sha1.o sds.o util.o anet.o log.o bankhall.pb.o codec.o  bankhall_skills.o epollreactor.o client.o 
main_objects = server.o customer.o telnetecho.o redis_proxy.o pp_client.o
sds_objects = zmalloc.o sha1.o sds.o util.o

ccserver : $(main_objects) $(objects)
	$(CC)  -o ccserver $(main_objects) $(objects) $(CPPFLAGS)   
    
ccpp_client : $(main_objects) $(objects)
	$(CC)  -o ccpp_client $(main_objects) $(objects) $(CPPFLAGS)
    
codec_test : $(objects) 
	$(CC)  -o codec_test $(objects)  $(CPPFLAGS) 

sds_test : $(sds_objects)
	$(CC)  -o sds_test $(sds_objects) $(CPPFLAGS)
    
all:$(objects) 

.PHONY : clean
clean :
	-rm $(objects) 
	-rm ccserver
