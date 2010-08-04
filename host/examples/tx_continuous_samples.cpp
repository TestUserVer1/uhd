//
// Copyright 2010 Ettus Research LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/simple_usrp.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread_time.hpp> //system time
#include <boost/format.hpp>
#include <iostream>
#include <complex>

namespace po = boost::program_options;

int UHD_SAFE_MAIN(int argc, char *argv[]){
    uhd::set_thread_priority_safe();

    //variables to be set by po
    std::string args;
    size_t total_duration;
    size_t samps_per_packet;
    double tx_rate, freq;
    float ampl;

    //setup the program options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("args", po::value<std::string>(&args)->default_value(""), "simple uhd device address args")
        ("duration", po::value<size_t>(&total_duration)->default_value(3), "number of seconds to transmit")
        ("spp", po::value<size_t>(&samps_per_packet)->default_value(1000), "number of samples per packet")
        ("txrate", po::value<double>(&tx_rate)->default_value(100e6/16), "rate of outgoing samples")
        ("freq", po::value<double>(&freq)->default_value(0), "rf center frequency in Hz")
        ("ampl", po::value<float>(&ampl)->default_value(float(0.3)), "amplitude of each sample")
        ("dilv", "specify to disable inner-loop verbose")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //print the help message
    if (vm.count("help")){
        std::cout << boost::format("UHD TX Continuous Samples %s") % desc << std::endl;
        return ~0;
    }

    bool verbose = vm.count("dilv") == 0;

    //create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args << std::endl;
    uhd::usrp::simple_usrp::sptr sdev = uhd::usrp::simple_usrp::make(args);
    uhd::device::sptr dev = sdev->get_device();
    std::cout << boost::format("Using Device: %s") % sdev->get_pp_string() << std::endl;

    //set properties on the device
    std::cout << boost::format("Setting TX Rate: %f Msps...") % (tx_rate/1e6) << std::endl;
    sdev->set_tx_rate(tx_rate);
    std::cout << boost::format("Actual TX Rate: %f Msps...") % (sdev->get_tx_rate()/1e6) << std::endl;
    sdev->set_tx_freq(freq);

    //allocate data to send
    std::vector<std::complex<float> > buff(samps_per_packet, std::complex<float>(ampl, ampl));

    //setup the metadata flags
    uhd::tx_metadata_t md;
    md.start_of_burst = true; //always SOB (good for continuous streaming)
    md.end_of_burst   = false;

    //send the data in multiple packets
    boost::system_time end_time(boost::get_system_time() + boost::posix_time::seconds(total_duration));
    while(end_time > boost::get_system_time()){
        //send samples per packet (driver fragments internally)
        size_t num_tx_samps = dev->send(
            &buff.front(), samps_per_packet, md,
            uhd::io_type_t::COMPLEX_FLOAT32,
            uhd::device::SEND_MODE_FULL_BUFF
        );
        if(verbose) std::cout << std::endl << boost::format("Sent %d samples") % num_tx_samps << std::endl;
    }

    //send a mini EOB packet
    if(verbose) std::cout << std::endl << boost::format("Sending packet with end-of-burst") << std::endl;
    md.start_of_burst = false;
    md.end_of_burst   = true;
    dev->send(
        NULL, 0, md,
        uhd::io_type_t::COMPLEX_FLOAT32,
        uhd::device::SEND_MODE_FULL_BUFF
    );

    //finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;

    return 0;
}
