
all:	test

test:	.test

.test:	$(wildcard *_test.py)
	python $? && touch .test
