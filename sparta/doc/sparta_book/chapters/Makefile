ADOC_FLAGS = --require asciidoctor-diagram \
             -a data-uri \
             -a toc=left \
             -a toclevels=3 \
             -a source-highlighter=rouge \
			 -a rouge-style=thankful_eyes \
             -a plantuml-jar-path=$(HOME)/plantuml/plantuml.jar

%.html: %.adoc Makefile
	asciidoctor $(ADOC_FLAGS) $<

book: sparta_book.adoc
	asciidoctor $(ADOC_FLAGS) sparta_book.adoc
