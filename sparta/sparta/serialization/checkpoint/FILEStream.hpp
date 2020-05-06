// <FILEStream> -*- C++ -*-

#pragma once

#include <ios>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <cstring>

/*! A class for creating an ostream from a C file stream.
 *
 * Create one of these on the stack, then use the \a getStream method
 * to get a reference to its std::ostream.
 *
 * This class basically holds the appropriate custom streambuf instance
 * and the ostream instance.  The caller will still be responsible for
 * creating and closing the C file stream.
 */
class FILEOstream {

private:

    /*! A streambuf for associating a std::ostream to a C file stream.
     *
     * The streambuf has an intermediate buffer (aka output sequence)
     * which may be used to buffer write data.  For details, see the
     * iostreams docs.
     *
     * This implementation does not support the following:
     *
     *   - Seeking (seekoff(), seekpos())
     */
    class OFILE_streambuf : public std::streambuf {
    public:

        /*! Constructor.
         *
         * \param fp FILE* associated with the streambuf
         * \param buf_size Size of the intermediate buffer in bytes
         *
         * \throw std::ios_base::failure If \a fp is NULL
         * \throw std::bad_alloc If unable to allocate intermediate buffer
         */
        OFILE_streambuf(FILE *fp, size_t buf_size = 1024) :
            fp_(fp),
            buf_(new char[buf_size]),
            buf_size_(buf_size)
        {
            if (fp_ == nullptr) {
                throw std::ios_base::failure("Underlying stream pointer is null");
            }
            if (buf_.get() == nullptr) {
                throw std::bad_alloc();
            }

            char *b = buf_.get();

            // Set current pointer to beginning of buffer
            setp(&b[0], &b[buf_size_]);
        }

    private:

        /*! Insert a character into the intermediate buffer.
         *
         * If it is full, this will first flush the intermediate buffer.
         *
         * \param ch Character to insert, or EOF.
         *
         * \return EOF on failure, or 0.
         */
        int_type overflow(int_type ch = traits_type::eof()) override {

            // Try to drain the intermediate buffer, if it is full.
            if (pptr() == epptr()) {
                sync();
            }
            if (traits_type::eq_int_type(ch, traits_type::eof()) == true) {
                return traits_type::eof();
            }

            // Insert the new character into the intermediate buffer.
            // Note: the pptr() may still be epptr(), if sync failed above.
            if (pptr() != epptr()) {
                *pptr() = ch;
                pbump(1);
                return 0;
            }
            return traits_type::eof();
        }

        /*! Flush the intermediate buffer to the output stream.
         *
         * \return 0 on success, -1 on failure.
         */
        int sync() override {
            size_t chars_to_write = pptr() - pbase();
            size_t chars_written = fwrite(pbase(), 1, chars_to_write, fp_);
            if (chars_written == chars_to_write) {
                char *b = buf_.get();
                setp(&b[0], &b[buf_size_]);
                return 0;
            }
            pbump(chars_written);
            return -1;
        }

        /*! Write chars from a user buffer to the output sequence.
         *
         * \param s Pointer to user buffer to write from
         * \param count Number of characters to write
         *
         * \return Number of characters successfully written.
         */
        std::streamsize xsputn(const char_type *s, std::streamsize count) override {
            // First flush out the intermediate buffer
            if (sync() == -1) {
                return 0;
            }

            // Now write the rest of the characters directly to the FILE
            return fwrite(s, 1, count, fp_);
        }

        FILE *fp_;                      //!< The underlying file stream
        std::unique_ptr<char[]> buf_;   //!< The intermediate buffer
        const size_t buf_size_;         //!< The size of the intermediate buffer
    };

public:

    /*! Constructor.
     *
     * \param fd Pointer to the underlying file stream.
     */
    FILEOstream(FILE *fd) :
        streambuf_(fd),
        stream_(&streambuf_) { }

    /*! Get a reference to the internal ostream.
     *
     * \return A reference to the internal ostream.
     *
     * \note Don't use it after 'this' goes is out of scope
     */
    std::ostream& getStream() {
        return stream_;
    }

private:

    OFILE_streambuf streambuf_;   //!< The streambuf between the ostream & the FD
    std::ostream stream_;         //!< The std::ostream object
};


/*! An class for creating an istream from a C file stream.
 *
 * Create one of these on the stack, then use the \a getStream method
 * to get a reference to its std::istream.
 *
 * This class basically holds the appropriate custom streambuf instance
 * and the istream instance.  The caller will still be responsible for
 * creating and closing the C file stream.
 */
class FILEIstream {

private:

    /*! A streambuf for associating a std::istream to a C file stream.
     *
     * The streambuf has an intermediate buffer (aka input sequence)
     * which it fills from the file stream.  For details, refer to the
     * iostreams docs.
     *
     * This implementation does not support the following:
     *
     *   - Seeking (seekoff(), seekpos())
     *   - re-syncing the file contents with the intermediate buffer (sync())
     *   - checking how many characters are still available (showmanyc())
     *   - Putting back characters, except for putting back the same character
     *     that was read from that position in the intermediate buffer (i.e.
     *     we can 'unget' the same characters that were read, back to the
     *     beginning of the intermediate buffer).
     */
    class IFILE_streambuf : public std::streambuf {
    public:

        /*! Constructor
         *
         * \param fp FILE* associated with the streambuf
         * \param buf_size Size of the intermediate buffer in bytes
         *
         * \throw std::ios_base::failure If \a fp is NULL
         * \throw std::bad_alloc If unable to allocate intermediate buffer
         */
        IFILE_streambuf(FILE *fp, size_t buf_size = 1024) :
            fp_(fp),
            buf_(new char[buf_size]),
            buf_size_(buf_size)
        {
            if (fp_ == nullptr) {
                throw std::ios_base::failure("Underlying stream pointer is null");
            }
            if (buf_.get() == nullptr) {
                throw std::bad_alloc();
            }

            char *b = buf_.get();

            // Set current pointer to end of buffer, indicating it is empty
            setg(&b[0], &b[buf_size_], &b[buf_size_]);
        }

    private:

        /*! Read characters into the intermediate buffer (if needed).
         *
         * This may be called during formatted or unformatted input.
         * It should refill the intermediate buffer, if it is empty.
         *
         * \return The value of the char pointed to by gptr(), on success;
         *         Traits::eof() otherwise.
         */
        int_type underflow() override {
            if (gptr() == egptr()) {
                char *b = buf_.get();
                auto byte_cnt = xsgetn(b, buf_size_);
                if (byte_cnt == 0) {
                    // There is no more data from the file, and the
                    // intermediate buffer is empty.  Leave gptr() == egptr(),
                    // as required by parent class.
                    return traits_type::eof();

                } else if (static_cast<size_t>(byte_cnt) < buf_size_) {
                    // We just extracted the last of the data from the file,
                    // and the intermediate buffer is partially filled.  Set
                    // the end pointer accordingly.
                    setg(&b[0], &b[0], &b[byte_cnt]);

                } else {
                    // We just filled the entire intermediate buffer, and there
                    // is still more data to read from the file.
                    setg(&b[0], &b[0], &b[buf_size_]);
                }
            }
            return *gptr();
        }

        /*! Read characters from the input sequence to a user buffer.
         *
         * \param s User buffer to read into
         * \param count Number of characters to read
         *
         * \return The number of characters successfully read.
         */
        std::streamsize xsgetn(char *s, std::streamsize count) override {
            // First empty the intermediate buffer.
            size_t intermediate_chars = egptr() - gptr();
            size_t intermediate_copy = std::min(intermediate_chars, static_cast<size_t>(count));
            memcpy(s, gptr(), intermediate_copy);
            gbump(intermediate_copy);  // Advance gptr()
            s += intermediate_copy;
            count -= intermediate_copy;
            size_t retval = intermediate_copy;

            // Now directly read from the FILE for the rest.
            retval += fread(s, 1, count, fp_);
            return retval;
        }

        FILE *fp_;                          //!< The underlying file stream
        std::unique_ptr<char[]> buf_;       //!< This is the intermediate buffer
        const size_t buf_size_;             //!< The size of the intermediate buf
    };

public:

    /*! Constructor.
     *
     * \param fd Pointer to the underlying file stream.
     */
    FILEIstream(FILE *fd) :
        streambuf_(fd),
        stream_(&streambuf_) { }

    /*! Get a reference to the internal istream.
     *
     * \return A reference to the internal istream.
     *
     * \note Don't use it after 'this' goes out of scope
     */
    std::istream& getStream() {
        return stream_;
    }

private:

    IFILE_streambuf streambuf_;   //!< The streambuf between the FD and istream
    std::istream stream_;         //!< The std::istream object
};


