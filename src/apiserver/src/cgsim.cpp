// Copyright 2018 Obelisk Inc.

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <ctime>
#include <iostream>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::literals;
using namespace std;
using boost::asio::ip::tcp;

io_service ioservice;

class Session {
public:
  Session(boost::asio::io_service &io_service) : socket_(io_service) {}

  tcp::socket &socket() { return socket_; }

  void start() {
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            boost::bind(&Session::handle_read, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
  }

  void handle_read(const boost::system::error_code &error, size_t bytes_transferred) {
    if (!error) {
      string command(data_);
      command = command.substr(0, bytes_transferred);
      cout << "Received command '" << command << "' len = " << command.length() << endl;
      if (command == "time") {
        std::time_t now = std::time(nullptr);
        string time_str(std::ctime(&now));
        socket_.async_send(buffer(time_str),
                           boost::bind(&Session::handle_write, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
      } else if (command == "version") {
        socket_.async_send(
            buffer("STATUS=S,When=123123,Code=10,Msg=Success,Description=Some description "
                   "goes here|4.37\n"),
            boost::bind(&Session::handle_write, this, boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
      } else {
        socket_.async_send(buffer("STATUS=E,When=999888,Code=0,Msg=Error?,Description=Derp!|\n"),
                           boost::bind(&Session::handle_write, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
      }

    } else {
      delete this;
    }
  }

  void handle_write(const boost::system::error_code &error, std::size_t bytes_transferred) {
    if (!error) {
      cout << "write complete of " << bytes_transferred << " bytes" << endl;
      socket_.async_read_some(boost::asio::buffer(data_, max_length),
                              boost::bind(&Session::handle_read, this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
      delete this;
    }
  }

private:
  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

class Server {
public:
  Server(boost::asio::io_service &io_service, short port)
      : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)) {
    Session *new_session = new Session(io_service_);
    acceptor_.async_accept(
        new_session->socket(),
        boost::bind(&Server::handle_accept, this, new_session, boost::asio::placeholders::error));
  }

  void handle_accept(Session *new_session, const boost::system::error_code &error) {
    if (!error) {
      cout << "New connection accepted" << endl;
      new_session->start();
      new_session = new Session(io_service_);
      acceptor_.async_accept(
          new_session->socket(),
          boost::bind(&Server::handle_accept, this, new_session, boost::asio::placeholders::error));
    } else {
      cout << "New connection error!" << error << endl;
      delete new_session;
    }
  }

private:
  boost::asio::io_service &io_service_;
  tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
  try {
    int port = 4028;
    if (argc == 2) {
      port = atoi(argv[1]);
    }

    boost::asio::io_service io_service;

    cout << "Running on port " << port << " ..." << endl;
    Server s(io_service, port);

    // boost::asio::io_service::work work(ioservice);

    io_service.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  cout << "Done." << endl;

  return 0;
}
