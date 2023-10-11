#include "Connection.hpp"
#include <iostream>

int main(int argc, char **argv) {
    Client client("15466.courses.cs.cmu.edu", "15466");
     {
            Connection &con = client.connection;
            con.send_buffer.emplace_back('H');
            con.send_buffer.emplace_back( 14 );
            con.send_buffer.emplace_back('g');
            for (char c : std::string("sonper")) {
                con.send_buffer.emplace_back(c);
            }
            for (char c : std::string("junruiz")) {
                con.send_buffer.emplace_back(c);
            }
        }

    while (true) {
        client.poll([](Connection *connection, Connection::Event evt) {
        },
        0.0
        );

        Connection &con = client.connection;
        while (true) {
            if (con.recv_buffer.size() < 2) break;
			char type = char(con.recv_buffer[0]);
			size_t length = size_t(con.recv_buffer[1]);
			if (con.recv_buffer.size() < 2 + length) break;

			if (type == 'T') {
				const char *as_chars = reinterpret_cast< const char * >(con.recv_buffer.data());
				std::string message(as_chars + 2, as_chars + 2 + length);

				std::cout << "T: '" << message << "'" << std::endl;
			} else {
				std::cout << "Ignored a " << type << " message of length " << length << "." << std::endl;
			}

			con.recv_buffer.erase(con.recv_buffer.begin(), con.recv_buffer.begin() + 2 + length);
        }
    }
}
