.PHONY: extension/observer.so clean

extension/observer.so:
	cd extension ; phpize && ./configure && make

php8trace:
	$(CC) php8trace.c -o php8trace

install: php8trace php8trace.sh extension/observer.so
	install php8trace /usr/local/bin
	install php8trace.sh /usr/local/bin
	cd extension ; make install

clean:
	rm -f php8trace
	( cd extension ; make clean )
