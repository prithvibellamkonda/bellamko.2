oss: user oss.c
	gcc -std=gnu99 oss.c -o oss

user: user.c
	gcc -std=gnu99 user.c -o user

clean:
	rm -f user oss
