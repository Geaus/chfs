#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "map_reduce/protocol.h"
#include "distributed/metadata_server.h"
#include "distributed/dataserver.h"
#include "distributed/client.h"

namespace mapReduce {
	int seq_duration;

	class MapReduceTest : public ::testing::Test {
		protected:
		        const std::string data_path = "/tmp/test_file";
		std::string outputStrings;
		const chfs::u16 meta_port = 8080;
		const chfs::u16 data_ports[3] = {
			8081, 8082, 8083
		}
		;
		const std::string inode_path = "/tmp/inode_file";
		const std::string data_path1 = "/tmp/test_file1";
		const std::string data_path2 = "/tmp/test_file2";
		const std::string data_path3 = "/tmp/test_file3";
		const std::string relative_path_to_novels = "../../test/map_reduce/novels/";
		std::shared_ptr<chfs::ChfsClient> client;
		std::shared_ptr<chfs::MetadataServer> meta_srv;
		std::shared_ptr<chfs::DataServer> data_srv1;
		std::shared_ptr<chfs::DataServer> data_srv2;
		std::shared_ptr<chfs::DataServer> data_srv3;
		std::shared_ptr<Coordinator> coordinator;
		std::shared_ptr<Worker> worker1;
		std::shared_ptr<Worker> worker2;
		std::shared_ptr<Worker> worker3;
		std::shared_ptr<Worker> worker4;
		std::vector<std::string> files = {
			"being_ernest.txt", "dorian_gray.txt", "frankenstein.txt", "metamorphosis.txt", "sherlock_holmes.txt", "tom_sawyer.txt"
		}
		;
		std::string outputFile_seq="mr-out-dis";
		std::string outputFile_dis="mr-out-seq";
		std::vector<std::pair<std::string, std::string>> correctKvs;

		void init_fs() {
			meta_srv = std::make_shared<chfs::MetadataServer>(meta_port, inode_path);
			data_srv1 = std::make_shared<chfs::DataServer>(data_ports[0], data_path1);
			data_srv2 = std::make_shared<chfs::DataServer>(data_ports[1], data_path2);
			data_srv3 = std::make_shared<chfs::DataServer>(data_ports[2], data_path3);
			client = std::make_shared<chfs::ChfsClient>();
			meta_srv->reg_server("127.0.0.1", data_ports[0], true);
			meta_srv->reg_server("127.0.0.1", data_ports[1], true);
			meta_srv->reg_server("127.0.0.1", data_ports[2], true);
			meta_srv->run();
			client->reg_server(chfs::ChfsClient::ServerType::METADATA_SERVER, "127.0.0.1",
			                               meta_port, true);
			client->reg_server(chfs::ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
			                               data_ports[0], true);
			client->reg_server(chfs::ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
			                               data_ports[1], true);
			client->reg_server(chfs::ChfsClient::ServerType::DATA_SERVER, "127.0.0.1",
			                               data_ports[2], true);
		}
		// This function is called before every test.
		void SetUp() override {
			init_fs();
			for (auto file : files) {
				std::ifstream f(relative_path_to_novels+file);
				if (!f.is_open()) {
					std::cerr << "open file error：" << relative_path_to_novels+file << std::endl;
					return;
				}
				std::stringstream buffer;
				buffer << f.rdbuf();
				std::string file_content = buffer.str();
				auto res_create = client->mknode(chfs::ChfsClient::FileType::REGULAR, 1, file);
				auto inode_id = res_create.unwrap();
				std::vector<uint8_t> vec(file_content.begin(), file_content.end());
				client->write_file(inode_id, 0, vec);
			} {
				std::ifstream f(relative_path_to_novels + "mr-wc-correct");
				if (!f.is_open()) {
					std::cerr << "open file error：" << std::endl;
					return;
				}
				std::string key, value;
				while (f >> key >> value)
				                    correctKvs.emplace_back(key, value);
				std::sort(correctKvs.begin(), correctKvs.end());
			}
		}
		// This function is called after every test.
		void TearDown() override {
			// Don't forget to remove the file to save memory
			meta_srv.reset();
			data_srv1.reset();
			data_srv2.reset();
			data_srv3.reset();
			std::remove(inode_path.c_str());
			std::remove(data_path1.c_str());
			std::remove(data_path2.c_str());
			std::remove(data_path3.c_str());
		}
		;
	}
	;
	TEST_F(MapReduceTest, SequentialMapReduce) {
		client->mknode(chfs::ChfsClient::FileType::REGULAR, 1, outputFile_seq);
		auto start = std::chrono::high_resolution_clock::now();
		SequentialMapReduce sequentialMapReduce(client, files, outputFile_seq);
		sequentialMapReduce.doWork();
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		seq_duration = static_cast<int>(duration.count());
		std::vector<std::pair<std::string, std::string>> kvs;
		auto res_lookup = client->lookup(1, outputFile_seq);
		auto inode_id = res_lookup.unwrap();
		auto res_type = client->get_type_attr(inode_id);
		auto length = res_type.unwrap().second.size;
		auto res_read = client->read_file(inode_id,0,length);
		auto char_vec = res_read.unwrap();
		std::string content(char_vec.begin(), char_vec.end());
		std::stringstream stringstream(content);
		std::string key, value;
		while (stringstream >> key >> value) {
			std::size_t first_not_null = key.find_first_not_of('\0');
			if (first_not_null == std::string::npos) {
				kvs.emplace_back("", value);
			}
			kvs.emplace_back(key.substr(first_not_null), value);
		}
		std::sort(kvs.begin(), kvs.end());
		EXPECT_EQ(kvs, correctKvs) << "incorrect output";
	}
	TEST_F(MapReduceTest, DistributedMapReduce) {
		client->mknode(chfs::ChfsClient::FileType::REGULAR, 1, outputFile_dis);
		MR_CoordinatorConfig config("127.0.0.1", 8084, client, outputFile_dis);
		auto start = std::chrono::high_resolution_clock::now();
		coordinator = std::make_shared<Coordinator>(config, files, 4);
		worker1 = std::make_shared<Worker>(config);
		worker2 = std::make_shared<Worker>(config);
		worker3 = std::make_shared<Worker>(config);
		worker4 = std::make_shared<Worker>(config);
		while (!coordinator->Done()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		worker1->stop();
		worker2->stop();
		worker3->stop();
		worker4->stop();
		std::vector<std::pair<std::string, std::string>> kvs;
		auto res_lookup = client->lookup(1, outputFile_dis);
		auto inode_id = res_lookup.unwrap();
		auto res_type = client->get_type_attr(inode_id);
		auto length = res_type.unwrap().second.size;
		auto res_read = client->read_file(inode_id,0,length);
		auto char_vec = res_read.unwrap();
		std::string content(char_vec.begin(), char_vec.end());
		std::stringstream stringstream(content);
		std::string key, value;
		while (stringstream >> key >> value) {
			std::size_t first_not_null = key.find_first_not_of('\0');
			if (first_not_null == std::string::npos) {
				kvs.emplace_back("", value);
			}
			kvs.emplace_back(key.substr(first_not_null), value);
		}
		std::sort(kvs.begin(), kvs.end());
		EXPECT_EQ(kvs, correctKvs) << "incorrect output";
		EXPECT_LE(static_cast<int>(duration.count()), 3 * seq_duration);
	}
}