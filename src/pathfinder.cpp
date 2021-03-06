#include "pathfinder.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <assert.h>
#include <sstream>
#include <ios>
#include <iostream>
#include <iomanip>
#include <stack>
#include <string>
#include <math.h>
#include <algorithm>


const char kPathSeparator =
#ifdef _WIN32
                            '\\';
#else
                            '/';
#endif

#define SSTR( x ) dynamic_cast< std::ostringstream & >( std::ostringstream() << std::dec << x ).str()

static std::ofstream label_file;
static std::ofstream stopids_file;

namespace fasttrips {

    const double PathFinder::MAX_COST = 999999;
    const double PathFinder::MAX_TIME = 999.999;

    /**
     * This doesn't really do anything.
     */
    PathFinder::PathFinder() : process_num_(-1), TIME_WINDOW_(-1), BUMP_BUFFER_(-1), STOCH_PATHSET_SIZE_(-1), STOCH_DISPERSION_(-1)
    {
    }

    void PathFinder::initializeParameters(
        double     time_window,
        double     bump_buffer,
        int        stoch_pathset_size,
        double     stoch_dispersion,
        int        stoch_max_stop_process_count)
    {
        TIME_WINDOW_                    = time_window;
        BUMP_BUFFER_                    = bump_buffer;
        STOCH_PATHSET_SIZE_             = stoch_pathset_size;
        STOCH_DISPERSION_               = stoch_dispersion;
        STOCH_MAX_STOP_PROCESS_COUNT_   = stoch_max_stop_process_count;
    }

    void PathFinder::readIntermediateFiles()
    {
        readTripIds();
        readStopIds();
        readRouteIds();
        readModeIds();
        readAccessLinks();
        readTransferLinks();
        readTripInfo();
        readWeights();
    }

    void PathFinder::readTripIds() {
        // Trips have been renumbered by fasttrips.  Read string IDs.
        // Trip num -> id
        std::ifstream trip_id_file;
        std::ostringstream ss_trip;
        ss_trip << output_dir_ << kPathSeparator << "ft_intermediate_trip_id.txt";
        trip_id_file.open(ss_trip.str().c_str(), std::ios_base::in);

        std::string string_trip_id_num, string_trip_id;
        int trip_id_num;

        trip_id_file >> string_trip_id_num >> string_trip_id;
        if (process_num_ <= 1) { 
            std::cout << "Reading " << ss_trip.str() << ": ";
            std::cout << "[" << string_trip_id_num   << "] ";
            std::cout << "[" << string_trip_id       << "] ";
        }
        while (trip_id_file >> trip_id_num >> string_trip_id) {
            trip_num_to_str_[trip_id_num] = string_trip_id;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << trip_num_to_str_.size() << " lines" << std::endl;
        }
        trip_id_file.close();
    }

    void PathFinder::readStopIds() {
        // Stops have been renumbered by fasttrips.  Read string IDs.
        // Stop num -> id
        std::ifstream stop_id_file;
        std::ostringstream ss_stop;
        ss_stop << output_dir_ << kPathSeparator << "ft_intermediate_stop_id.txt";
        stop_id_file.open(ss_stop.str().c_str(), std::ios_base::in);

        std::string string_stop_id_num, string_stop_id;
        int stop_id_num;

        stop_id_file >> string_stop_id_num >> string_stop_id;
        if (process_num_ <= 1) { 
            std::cout << "Reading " << ss_stop.str() << ": ";
            std::cout << "[" << string_stop_id_num   << "] ";
            std::cout << "[" << string_stop_id       << "] ";
        }
        while (stop_id_file >> stop_id_num >> string_stop_id) {
            stop_num_to_str_[stop_id_num] = string_stop_id;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << stop_num_to_str_.size() << " lines" << std::endl;
        }
        stop_id_file.close();
    }

    void PathFinder::readRouteIds() {
        // Routes have been renumbered by fasttrips. Read string IDs.
        // Route num -> id
        std::ifstream route_id_file;
        std::ostringstream ss_route;
        ss_route << output_dir_ << kPathSeparator << "ft_intermediate_route_id.txt";
        route_id_file.open(ss_route.str().c_str(), std::ios_base::in);

        std::string string_route_id_num, string_route_id;
        int route_id_num;

        route_id_file >> string_route_id_num >> string_route_id;
        if (process_num_ <= 1) { 
            std::cout << "Reading " << ss_route.str() << ": ";
            std::cout << "[" << string_route_id_num   << "] ";
            std::cout << "[" << string_route_id       << "] ";
        }
        while (route_id_file >> route_id_num >> string_route_id) {
            route_num_to_str_[route_id_num] = string_route_id;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << route_num_to_str_.size() << " lines" << std::endl;
        }
        route_id_file.close();
    }

    void PathFinder::readModeIds() {
        // Supply modes have been renumbered by fasttrips. Read string IDs.
        // Supply mode num -> id
        std::ifstream mode_id_file;
        std::ostringstream ss_mode;
        ss_mode << output_dir_ << kPathSeparator << "ft_intermediate_supply_mode_id.txt";
        mode_id_file.open(ss_mode.str().c_str(), std::ios_base::in);

        std::string string_mode_num, string_mode;
        int mode_num;

        mode_id_file >> string_mode_num >> string_mode;
        if (process_num_ <= 1) {
            std::cout << "Reading " << ss_mode.str() << ": ";
            std::cout << "[" << string_mode_num      << "] ";
            std::cout << "[" << string_mode          << "] ";
        }
        while (mode_id_file >> mode_num >> string_mode) {
            mode_num_to_str_[mode_num] = string_mode;
            if (string_mode == "transfer") { transfer_supply_mode_ = mode_num; }
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << mode_num_to_str_.size() << " lines" << std::endl;
        }
        mode_id_file.close();
    }

    void PathFinder::readAccessLinks() {
        // Taz Access and Egress links (various supply modes)
        std::ifstream acceggr_file;
        std::ostringstream ss_accegr;
        ss_accegr << output_dir_ << kPathSeparator << "ft_intermediate_access_egress.txt";
        acceggr_file.open(ss_accegr.str().c_str(), std::ios_base::in);


        std::string string_taz_num, string_supply_mode_num, string_stop_id_num, attr_name, string_attr_value;
        int taz_num, supply_mode_num, stop_id_num;
        double attr_value;

        acceggr_file >> string_taz_num >> string_supply_mode_num >> string_stop_id_num >> attr_name >> string_attr_value;
        if (process_num_ <= 1) {
            std::cout << "Reading " << ss_accegr.str() << ": ";
            std::cout << "[" << string_taz_num         << "] ";
            std::cout << "[" << string_supply_mode_num << "] ";
            std::cout << "[" << string_stop_id_num     << "] ";
            std::cout << "[" << attr_name              << "] ";
            std::cout << "[" << string_attr_value      << "] ";
        }
        int attrs_read = 0;
        while (acceggr_file >> taz_num >> supply_mode_num >> stop_id_num >> attr_name >> attr_value) {
            taz_access_links_[taz_num][supply_mode_num][stop_id_num][attr_name] = attr_value;
            attrs_read++;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << attrs_read << " lines" << std::endl;
        }
        acceggr_file.close();
    }

    void PathFinder::readTransferLinks() {
        // Transfer links
        std::ifstream transfer_file;
        std::ostringstream ss_transfer;
        ss_transfer << output_dir_ << kPathSeparator << "ft_intermediate_transfers.txt";
        transfer_file.open(ss_transfer.str().c_str(), std::ios_base::in);

        std::string string_from_stop_id_num, string_to_stop_id_num, attr_name, string_attr_value;
        int from_stop_id_num, to_stop_id_num;
        double attr_value;

        transfer_file >> string_from_stop_id_num >> string_to_stop_id_num >> attr_name >> string_attr_value;
        if (process_num_ <= 1) {
            std::cout << "Reading " << ss_transfer.str() << ": ";
            std::cout << "[" << string_from_stop_id_num  << "] ";
            std::cout << "[" << string_to_stop_id_num    << "] ";
            std::cout << "[" << attr_name                << "] ";
            std::cout << "[" << string_attr_value        << "] ";
        }
        int attrs_read = 0;
        while (transfer_file >> from_stop_id_num >> to_stop_id_num >> attr_name >> attr_value) {
            // o -> d -> attrs
            transfer_links_o_d_[from_stop_id_num][to_stop_id_num][attr_name] = attr_value;

            // d -> o -> attrs
            transfer_links_d_o_[to_stop_id_num][from_stop_id_num][attr_name] = attr_value;
            attrs_read++;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << attrs_read << " lines" << std::endl;
        }
        transfer_file.close();
    }

    void PathFinder::readTripInfo() {
        std::ifstream tripinfo_file;
        std::ostringstream ss_tripinfo;
        ss_tripinfo << output_dir_ << kPathSeparator << "ft_intermediate_trip_info.txt";
        tripinfo_file.open(ss_tripinfo.str().c_str(), std::ios_base::in);

        std::string string_trip_id_num, attr_name, string_attr_value;
        int trip_id_num;
        double attr_value;

        tripinfo_file >> string_trip_id_num >> attr_name >> string_attr_value;
        if (process_num_ <= 1) {
            std::cout << "Reading " << ss_tripinfo.str() << ": ";
            std::cout << "[" << string_trip_id_num       << "] ";
            std::cout << "[" << attr_name                << "] ";
            std::cout << "[" << string_attr_value        << "] ";
        }
        int attrs_read = 0;
        while (tripinfo_file >> trip_id_num >> attr_name >> attr_value) {

            // these are special
            if (attr_name == "mode_num") {
                trip_info_[trip_id_num].supply_mode_num_ = int(attr_value);
            } else if (attr_name == "route_id_num") {
                trip_info_[trip_id_num].route_id_ = int(attr_value);
            } else {
                trip_info_[trip_id_num].trip_attr_[attr_name] = attr_value;
            }
            attrs_read++;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << attrs_read << " lines" << std::endl;
        }
        tripinfo_file.close();
    }

    void PathFinder::readWeights() {
        // Weights
        std::ifstream weights_file;
        std::ostringstream ss_weights;
        ss_weights << output_dir_ << kPathSeparator << "ft_intermediate_weights.txt";
        weights_file.open(ss_weights.str().c_str(), std::ios_base::in);

        std::string user_class, demand_mode_type, demand_mode, string_supply_mode_num, weight_name, string_weight_value;
        int supply_mode_num;
        double weight_value;

        weights_file >> user_class >> demand_mode_type >> demand_mode >> string_supply_mode_num >> weight_name >> string_weight_value;
        if (process_num_ <= 1) {
            std::cout << "Reading " << ss_weights.str() << ": ";
            std::cout << "[" << user_class              << "] ";
            std::cout << "[" << demand_mode_type        << "] ";
            std::cout << "[" << demand_mode             << "] ";
            std::cout << "[" << string_supply_mode_num  << "] ";
            std::cout << "[" << weight_name             << "] ";
            std::cout << "[" << string_weight_value     << "] ";
        }
        int weights_read = 0;
        while (weights_file >> user_class >> demand_mode_type >> demand_mode >> supply_mode_num >> weight_name >> weight_value) {
            UserClassMode ucm = { user_class, fasttrips::MODE_ACCESS, demand_mode };
            if      (demand_mode_type == "access"  ) { ucm.demand_mode_type_ = MODE_ACCESS;  }
            else if (demand_mode_type == "egress"  ) { ucm.demand_mode_type_ = MODE_EGRESS;  }
            else if (demand_mode_type == "transit" ) { ucm.demand_mode_type_ = MODE_TRANSIT; }
            else if (demand_mode_type == "transfer") { ucm.demand_mode_type_ = MODE_TRANSFER;}
            else {
                std::cerr << "Do not understand demand_mode_type [" << demand_mode_type << "] in " << ss_weights.str() << std::endl;
                exit(2);
            }

            weight_lookup_[ucm][supply_mode_num][weight_name] = weight_value;
            weights_read++;
        }
        if (process_num_ <= 1) {
            std::cout << " => Read " << weights_read << " lines" << std::endl;
        }
        weights_file.close();
    }

    void PathFinder::initializeSupply(
        const char* output_dir,
        int         process_num,
        int*        stoptime_index,
        double*     stoptime_times,
        int         num_stoptimes)
    {
        output_dir_  = output_dir;
        process_num_ = process_num;
        readIntermediateFiles();

        for (int i=0; i<num_stoptimes; ++i) {
            TripStopTime stt = {
                stoptime_index[3*i],    // trip id
                stoptime_index[3*i+1],  // sequence
                stoptime_index[3*i+2],  // stop id
                stoptime_times[2*i],    // arrive time
                stoptime_times[2*i+1]   // depart time
            };
            // verify the sequence number makes sense: sequential, starts with 1
            assert(stt.sequence_ == trip_stop_times_[stt.trip_id_].size()+1);

            trip_stop_times_[stt.trip_id_].push_back(stt);
            stop_trip_times_[stt.stop_id_].push_back(stt);
            if (false && (process_num <= 1) && ((i<5) || (i>num_stoptimes-5))) {
                printf("stoptimes[%4d][%4d][%4d]=%f, %f\n", stoptime_index[3*i], stoptime_index[3*i+1], stoptime_index[3*i+2],
                       stoptime_times[2*i], stoptime_times[2*i+1]);
            }
        }
    }

    void PathFinder::setBumpWait(int*       bw_index,
                                 double*    bw_data,
                                 int        num_bw)
    {
        for (int i=0; i<num_bw; ++i) {
            TripStop ts = { bw_index[3*i], bw_index[3*i+1], bw_index[3*i+2] };
            bump_wait_[ts] = bw_data[i];
            if (true && (process_num_ <= 1) && ((i<5) || (i>num_bw-5))) {
                printf("bump_wait[%6d %6d %6d] = %f\n",
                       bw_index[3*i], bw_index[3*i+1], bw_index[3*i+2], bw_data[i] );
            }
        }
    }

    /// This doesn't really do anything because the instance variables are all STL structures
    /// which take care of freeing memory.
    PathFinder::~PathFinder()
    {
        // std::cout << "PathFinder destructor" << std::endl;
    }

    void PathFinder::findPath(PathSpecification path_spec,
                              Path              &path,
                              PathInfo          &path_info,
                              PerformanceInfo   &performance_info) const
    {
        // for now we'll just trace
        // if (!path_spec.trace_) { return; }

        std::ofstream trace_file;
        if (path_spec.trace_) {
            std::ostringstream ss;
            ss << output_dir_ << kPathSeparator;
            ss << "fasttrips_trace_" << path_spec.path_id_ << ".log";
            // append because this will happen across iterations
            std::ios_base::openmode omode = std::ios_base::out;
            if (path_spec.iteration_ != 1) {
                omode = omode | std::ios_base::app; // append
            }
            trace_file.open(ss.str().c_str(), omode);
            trace_file << "Tracing assignment of passenger " << path_spec.passenger_id_ << " with path id " << path_spec.path_id_ << std::endl;
            trace_file << "iteration_       = " << path_spec.iteration_ << std::endl;
            trace_file << "outbound_        = " << path_spec.outbound_  << std::endl;
            trace_file << "hyperpath_       = " << path_spec.hyperpath_ << std::endl;
            trace_file << "preferred_time_  = ";
            printTime(trace_file, path_spec.preferred_time_);
            trace_file << " (" << path_spec.preferred_time_ << ")" << std::endl;
            trace_file << "user_class_      = " << path_spec.user_class_   << std::endl;
            trace_file << "access_mode_     = " << path_spec.access_mode_  << std::endl;
            trace_file << "transit_mode_    = " << path_spec.transit_mode_ << std::endl;
            trace_file << "egress_mode_     = " << path_spec.egress_mode_  << std::endl;
            trace_file << "orig_taz_id_     = " << path_spec.origin_taz_id_      << std::endl;
            trace_file << "dest_taz_id_     = " << path_spec.destination_taz_id_ << std::endl;

            std::ostringstream ss2;
            ss2 << output_dir_ << kPathSeparator;
            ss2 << "fasttrips_labels_ids_" << path_spec.path_id_ << ".csv";
            stopids_file.open(ss2.str().c_str(), omode);
            stopids_file << "stop_id,stop_id_label_iter" << std::endl;
        }

        StopStates           stop_states;
        LabelStopQueue       label_stop_queue;
        HyperpathStopStates  hyperpath_ss;

#ifdef _WIN32
        // QueryPerformanceFrequency reference: https://msdn.microsoft.com/en-us/library/windows/desktop/dn553408(v=vs.85).aspx
        LARGE_INTEGER        frequency;
        LARGE_INTEGER        labeling_start_time, labeling_end_time, pathfind_end_time;
        LARGE_INTEGER        label_elapsed, pathfind_elapsed;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&labeling_start_time);
#else
        // using gettimeofday() since std::chrono is only c++11
        struct timeval       labeling_start_time, labeling_end_time, pathfind_end_time;
        gettimeofday(&labeling_start_time, NULL);
#endif

        // todo: handle failure
        bool success = initializeStopStates(path_spec, trace_file, stop_states, label_stop_queue, hyperpath_ss);

        performance_info.label_iterations_ = labelStops(path_spec, trace_file, stop_states, label_stop_queue, hyperpath_ss, performance_info.max_process_count_);

        std::vector<StopState> taz_state;
        finalizeTazState(path_spec, trace_file, stop_states, label_stop_queue, performance_info.label_iterations_, hyperpath_ss);

#ifdef _WIN32
        QueryPerformanceCounter(&labeling_end_time);
#else
        gettimeofday(&labeling_end_time, NULL);
#endif

        getFoundPath(path_spec, trace_file, stop_states, hyperpath_ss, path, path_info);

#ifdef _WIN32
        QueryPerformanceCounter(&pathfind_end_time);

        label_elapsed.QuadPart                = labeling_end_time.QuadPart - labeling_start_time.QuadPart;
        pathfind_elapsed.QuadPart             = pathfind_end_time.QuadPart - labeling_end_time.QuadPart;

        // We now have the elapsed number of ticks, along with the
        // number of ticks-per-second. We use these values
        // to convert to the number of elapsed milliseconds.
        // To guard against loss-of-precision, we convert
        // to microseconds *before* dividing by ticks-per-second.
        label_elapsed.QuadPart    *= 1000;
        label_elapsed.QuadPart    /= frequency.QuadPart;
        pathfind_elapsed.QuadPart *= 1000;
        pathfind_elapsed.QuadPart /= frequency.QuadPart;

        performance_info.milliseconds_labeling_    = (long)label_elapsed.QuadPart;
        performance_info.milliseconds_enumerating_ = (long)pathfind_elapsed.QuadPart;
#else
        gettimeofday(&pathfind_end_time, NULL);

        // microseconds
        long int diff = (labeling_end_time.tv_usec   + 1000000*labeling_end_time.tv_sec) -
                        (labeling_start_time.tv_usec + 1000000*labeling_start_time.tv_sec);
        performance_info.milliseconds_labeling_ = 0.001*diff;

        diff = (pathfind_end_time.tv_usec   + 1000000*pathfind_end_time.tv_sec) -
               (labeling_end_time.tv_usec   + 1000000*labeling_end_time.tv_sec);
        performance_info.milliseconds_enumerating_ = 0.001*diff;
#endif

        if (path_spec.trace_) {

            trace_file << "        label iterations: " << performance_info.label_iterations_    << std::endl;
            trace_file << "       max process count: " << performance_info.max_process_count_   << std::endl;
            trace_file << "   milliseconds labeling: " << performance_info.milliseconds_labeling_    << std::endl;
            trace_file << "milliseconds enumerating: " << performance_info.milliseconds_enumerating_ << std::endl;
            trace_file.close();
            label_file.close();
            stopids_file.close();
        }
    }

    double PathFinder::tallyLinkCost(
        const int supply_mode_num,
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        const NamedWeights& weights,
        const Attributes& attributes) const
    {
        // iterate through the weights
        double cost = 0;
        if (true && path_spec.trace_) {
            trace_file << "Link cost for " << std::setw(15) << std::setfill(' ') << std::left << mode_num_to_str_.find(supply_mode_num)->second;
            trace_file << std::setw(15) << std::setfill(' ') << std::right << "weight" << " x attribute" <<std::endl;
        }

        NamedWeights::const_iterator iter_weights;
        for (iter_weights  = weights.begin();
             iter_weights != weights.end(); ++iter_weights) {

            // look for the attribute
            Attributes::const_iterator iter_attr = attributes.find(iter_weights->first);
            if (iter_attr == attributes.end()) {
                // error out??
                if (path_spec.trace_) {
                    trace_file << " => NO ATTRIBUTE CALLED " << iter_weights->first << std::endl;
                }
                std::cerr << " => NO ATTRIBUTE CALLED " << iter_weights->first << std::endl;
                continue;
            }

            cost += iter_weights->second * iter_attr->second;
            if (true && path_spec.trace_) {
                trace_file << std::setw(26) << std::setfill(' ') << std::right << iter_weights->first << ":  + ";
                trace_file << std::setw(13) << std::setprecision(4) << std::fixed << iter_weights->second;
                trace_file << " x " << iter_attr->second << std::endl;
            }
        }
        if (true && path_spec.trace_) {
            trace_file << std::setw(26) << std::setfill(' ') << "final cost" << ":  = ";
            trace_file << std::setw(13) << std::setprecision(4) << std::fixed << cost << std::endl;
        }
        return cost;
    }

    void PathFinder::addStopState(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        const int stop_id,
        const StopState& ss,
        StopStates& stop_states,
        LabelStopQueue& label_stop_queue,
        HyperpathStopStates& hyperpath_ss) const
    {
        // do we even want to incorporate this link to our stop state?
        bool rejected = false;

        std::string trace_suffix;

        // for deterministic, this is simple
        if (!path_spec.hyperpath_) {
            LabelStop ls = { ss.cost_, stop_id };

            trace_suffix = " (rejected)";
            rejected     = true;

            // new stop state -- just add it
            if (stop_states[stop_id].size() == 0) {
                stop_states[stop_id].push_back(ss);

                // push this stop and it's cost for processing
                label_stop_queue.push( ls );

                trace_suffix = " (new)";
                rejected     = false;
            }
            // if the stop state exists already, check if this cost is lower
            else if (ss.cost_ < stop_states[stop_id][0].cost_) {
                stop_states[stop_id][0] = ss;  // replace

                // push this stop and it's cost for processing
                label_stop_queue.push( ls );

                trace_suffix = " (update)";
                rejected     = false;
            }
        }

        else {
            LabelStop ls = { ss.cost_, stop_id };

            // Just set if it's new
            // However, if it's bigger than MAX_COST, that's problematic 
            if (hyperpath_ss.find(stop_id) == hyperpath_ss.end()) {
                HyperpathState hss =  { ss.deparr_time_, ss.trip_id_, ss.cost_, 0 };
                hyperpath_ss[stop_id] = hss;

                stop_states[stop_id].push_back(ss);
                label_stop_queue.push( ls );

                trace_suffix = " (new)";
            }

            else
            {

                // keep track if the state changed (label or time window)
                // if so, we'll want to trigger dealing with the effects by adding it to the queue
                bool update_state = false;

                // Update stop state
                HyperpathState& hss = hyperpath_ss[stop_id];

                // is it too early (outbound) or too late (inbound)?
                if (( path_spec.outbound_ && (ss.deparr_time_ < hss.latest_dep_earliest_arr_ - TIME_WINDOW_)) ||
                    (!path_spec.outbound_ && (ss.deparr_time_ > hss.latest_dep_earliest_arr_ + TIME_WINDOW_))) {
                    rejected = true;
                    trace_suffix = " (rejected)";
                }

                // update latest departure time or earliest arrival time window
                if (( path_spec.outbound_ && (ss.deparr_time_ > hss.latest_dep_earliest_arr_)) ||
                    (!path_spec.outbound_ && (ss.deparr_time_ < hss.latest_dep_earliest_arr_))) {
                    hss.latest_dep_earliest_arr_ = ss.deparr_time_;
                    hss.lder_trip_id_            = ss.trip_id_;
                    update_state                 = true;
                    ls.label_                    = hss.hyperpath_cost_;
                    trace_suffix                += " (window)";
                }

                // update stop cost if it's affected
                if (!rejected) {

                    // see if it's there already and add it if not, and recalculate hyperpath cost
                    bool stop_state_found = false;
                    double hyperpath_cost = 0.0;
                    std::stack<int> prune_indices;

                    for (int ss_index=0; ss_index < stop_states[stop_id].size(); ++ss_index) {

                        // see if it's there already
                        StopState& ss_check = stop_states[stop_id][ss_index];
                        if ((ss_check.deparr_mode_   == ss.deparr_mode_  ) &&
                            (ss_check.trip_id_       == ss.trip_id_      ) &&
                            (ss_check.stop_succpred_ == ss.stop_succpred_) &&
                            (ss_check.seq_succpred_  == ss.seq_succpred_ )) {
                            // log it
                            trace_suffix += " (sub)";
                            if (false && path_spec.trace_) {
                                trace_file << "Updating stop state:" << std::endl;
                                trace_file << "        ";
                                printStopState(trace_file, stop_id, ss_check, path_spec);
                                trace_file << " (orig)" << std::endl;
                                trace_file << "        ";
                                printStopState(trace_file, stop_id, ss, path_spec);
                                trace_file << " (new)" << std::endl;
                            }
                            // replace it with updated version
                            stop_states[stop_id][ss_index] = ss;
                            stop_state_found = true;
                        }

                        // TODO: window-pruning?  prune stop states outside the window??
                        if (( path_spec.outbound_ && (stop_states[stop_id][ss_index].deparr_time_ < hss.latest_dep_earliest_arr_ - TIME_WINDOW_)) ||
                            (!path_spec.outbound_ && (stop_states[stop_id][ss_index].deparr_time_ > hss.latest_dep_earliest_arr_ + TIME_WINDOW_))) {
                            prune_indices.push(ss_index);
                        }
                        // otherwise tally the cost
                        else {
                            hyperpath_cost += exp(-1.0*STOCH_DISPERSION_*stop_states[stop_id][ss_index].cost_);
                        }
                    }

                    // window-pruning
                    while (!prune_indices.empty()) {
                        int prune_index = prune_indices.top();

                        if (path_spec.trace_) {
                            trace_file << "  + del ";
                            printStopState(trace_file, stop_id, stop_states[stop_id][prune_index], path_spec);
                            trace_file << " (prune-window)" << std::endl;
                        }

                        stop_states[stop_id].erase( stop_states[stop_id].begin() + prune_index );
                        prune_indices.pop();
                    }

                    if (!stop_state_found) {
                        // add it to stop_states
                        stop_states[stop_id].push_back(ss);
                        hyperpath_cost += exp(-1.0*STOCH_DISPERSION_*ss.cost_);
                    }

                    // update the hyperpath cost if it's changed
                    hyperpath_cost = (-1.0/STOCH_DISPERSION_)*log(hyperpath_cost);
                    if (abs(hyperpath_cost - hss.hyperpath_cost_) > 0.0001) {
                        if (path_spec.trace_) {
                            std::ostringstream oss;
                            oss << " (hp cost " << std::setprecision(4) << std::fixed << hss.hyperpath_cost_ << "->" << hyperpath_cost << ")";
                            trace_suffix += oss.str();
                        }
                        update_state        = true;
                        hss.hyperpath_cost_ = hyperpath_cost;
                        ls.label_           = hyperpath_cost;
                    }

                    if (update_state) {
                        // push this stop and it's departure time / arrival time for processing
                        label_stop_queue.push( ls );
                    }
                }
            }
        }

        // the rest is for debugging
        if (!path_spec.trace_) {
            return;
        }

        // log it to trace
        trace_file << "  + new ";
        printStopState(trace_file, stop_id, ss, path_spec);
        trace_file << trace_suffix << std::endl;

        if (rejected) { return; }

        static int link_num = 1;        // unique ID for the link
        static int last_iter = -1;   // last stop id that got numbered

        if (!label_file.is_open()) {
            link_num = 1;  // reset

            std::ostringstream ss;
            ss << output_dir_ << kPathSeparator;
            ss << "fasttrips_labels_" << path_spec.path_id_ << ".csv";
            label_file.open(ss.str().c_str(), (path_spec.iteration_ == 1 ? std::ios_base::out : std::ios_base::out | std::ios_base::app));
            label_file << "label_iteration,link,node ID,time,mode,trip_id,link_time,link_cost,cost,AB" << std::endl;
        }

        // write the labels out to the label csv
        for (int o_d = 0; o_d < 2; ++o_d) {
            // print it into the labels file
            label_file << ss.iteration_ << ",";
            label_file << link_num      << ",";

            if (o_d == 0) { label_file << stop_num_to_str_.find(stop_id)->second << ","; }
            else          { label_file << stop_num_to_str_.find(ss.stop_succpred_)->second << ","; }

            if (o_d == 0) { label_file << ss.deparr_time_ << ","; }
            else          { label_file << ss.arrdep_time_ << ","; }

            // mode
            printMode(label_file, ss.deparr_mode_, ss.trip_id_);
            label_file << ",";

            // trip id
            if (ss.deparr_mode_ == MODE_TRANSIT) {
                label_file << trip_num_to_str_.find(ss.trip_id_)->second << ",";
            } else {
                label_file << mode_num_to_str_.find(ss.trip_id_)->second << ",";
            }
            label_file << ss.link_time_ << ",";
            label_file << ss.link_cost_ << ",";
            label_file << std::fixed << ss.cost_ << ",";
            if      ( path_spec.outbound_ && o_d == 0) { label_file << "A" << std::endl; }
            else if (!path_spec.outbound_ && o_d == 1) { label_file << "A" << std::endl; }
            else                                       { label_file << "B" << std::endl; }
        }
        ++link_num;
    }

    bool PathFinder::initializeStopStates(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        StopStates& stop_states,
        LabelStopQueue& label_stop_queue,
        HyperpathStopStates& hyperpath_ss) const
    {
        int     start_taz_id = path_spec.outbound_ ? path_spec.destination_taz_id_ : path_spec.origin_taz_id_;
        double  dir_factor   = path_spec.outbound_ ? 1.0 : -1.0;

        // are there any egress/access links for this TAZ?
        TAZSupplyStopToAttr::const_iterator iter_tss2a = taz_access_links_.find(start_taz_id);
        if (iter_tss2a == taz_access_links_.end()) {
            return false;
        }

        // Are there any supply modes for this demand mode?
        UserClassMode ucm = { path_spec.user_class_,
                              path_spec.outbound_ ? MODE_EGRESS: MODE_ACCESS,
                              path_spec.outbound_ ? path_spec.egress_mode_ : path_spec.access_mode_
                            };
        WeightLookup::const_iterator iter_weights = weight_lookup_.find(ucm);
        if (iter_weights == weight_lookup_.end()) {
            std::cerr << "Couldn't find any weights configured for user class [" << path_spec.user_class_ << "], ";
            std::cerr << (path_spec.outbound_ ? "egress mode [" : "access mode [");
            std::cerr << (path_spec.outbound_ ? path_spec.egress_mode_ : path_spec.access_mode_) << "]" << std::endl;
            return false;
        }

        if (path_spec.trace_) {
            stopids_file << stop_num_to_str_.find(start_taz_id)->second << ",0" << std::endl;
        }

        // Iterate through valid supply modes
        SupplyModeToNamedWeights::const_iterator iter_s2w;
        for (iter_s2w  = iter_weights->second.begin();
             iter_s2w != iter_weights->second.end(); ++iter_s2w) {
            int supply_mode_num = iter_s2w->first;

            if (path_spec.trace_) {
                trace_file << "Weights exist for supply mode " << supply_mode_num << " => ";
                trace_file << mode_num_to_str_.find(supply_mode_num)->second << std::endl;
            }

            // Are there any egress/access links for the supply mode?
            SupplyStopToAttr::const_iterator iter_ss2a = iter_tss2a->second.find(supply_mode_num);
            if (iter_ss2a == iter_tss2a->second.end()) {
                if (path_spec.trace_) {
                    trace_file << "No links for this supply mode" << std::endl;
                }
                continue;
            }

            // Iterate through the links for the given supply mode
            StopToAttr::const_iterator link_iter;
            for (link_iter  = iter_ss2a->second.begin();
                 link_iter != iter_ss2a->second.end(); ++link_iter)
            {
                int stop_id = link_iter->first;
                Attributes link_attr = link_iter->second;
                double attr_time = link_attr.find("time_min")->second;

                // outbound: departure time = destination - access
                // inbound:  arrival time   = origin      + access
                double deparr_time = path_spec.preferred_time_ - (attr_time*dir_factor);
                // we start out with no delay
                link_attr["preferred_delay_min"] = 0.0;

                double cost;
                if (path_spec.hyperpath_) {
                    cost = tallyLinkCost(supply_mode_num, path_spec, trace_file, iter_s2w->second, link_attr);
                } else {
                    cost = attr_time;
                }

                StopState ss = {
                    deparr_time,                                                                // departure/arrival time
                    path_spec.outbound_ ? MODE_EGRESS : MODE_ACCESS,                            // departure/arrival mode
                    supply_mode_num,                                                            // trip id
                    start_taz_id,                                                               // successor/predecessor
                    -1,                                                                         // sequence
                    -1,                                                                         // sequence succ/pred
                    attr_time,                                                                  // link time
                    cost,                                                                       // link cost
                    cost,                                                                       // cost
                    0,                                                                          // iteration
                    path_spec.preferred_time_ };                                                 // arrival/departure time
                addStopState(path_spec, trace_file, stop_id, ss, stop_states, label_stop_queue, hyperpath_ss);

            } // end iteration through links for the given supply mode
        } // end iteration through valid supply modes

        if (label_stop_queue.size() > 0)
            return true;
        return false;
    }

    /**
     * Part of the labeling loop. Assuming the *current_label_stop* was just pulled off the
     * *label_stop_queue*, this method will iterate through transfers to (for outbound) or
     * from (for inbound) the current stop and update the next stop given the current stop state.
     **/
    void PathFinder::updateStopStatesForTransfers(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        StopStates& stop_states,
        LabelStopQueue& label_stop_queue,
        HyperpathStopStates& hyperpath_ss,
        int label_iteration,
        const LabelStop& current_label_stop) const
    {
        double dir_factor = path_spec.outbound_ ? 1.0 : -1.0;

        // current_stop_state is a vector
        std::vector<StopState>& current_stop_state = stop_states[current_label_stop.stop_id_];
        int current_mode = current_stop_state[0].deparr_mode_;      // why index 0?
        int current_trip = current_stop_state[0].trip_id_;
        double latest_dep_earliest_arr = current_stop_state[0].deparr_time_;

        // no transfer to/from access or egress
        if (current_mode == MODE_EGRESS) return;
        if (current_mode == MODE_ACCESS) return;
        // if not hyperpath, transfer not ok
        if (!path_spec.hyperpath_ && current_mode == MODE_TRANSFER) return;


        double nonwalk_label = 0;
        if (path_spec.hyperpath_) {
            latest_dep_earliest_arr = hyperpath_ss[current_label_stop.stop_id_].latest_dep_earliest_arr_;
            nonwalk_label = calculateNonwalkLabel(current_stop_state);
            if (path_spec.trace_) { trace_file << "  nonwalk label:    " << nonwalk_label << std::endl; }

            // if nonwalk label == MAX_COST then the only way to reach this stop is via transfer so we don't want to transfer again
            if (nonwalk_label == PathFinder::MAX_COST) return;
        }
        // are there relevant transfers?
        StopStopToAttr::const_iterator transfer_map_it;
        bool found_transfers = false;
        if (path_spec.outbound_) {
            // if outbound, going backwards, so transfer TO this current stop
            transfer_map_it = transfer_links_d_o_.find(current_label_stop.stop_id_);
            found_transfers = (transfer_map_it != transfer_links_d_o_.end());
        } else {
            // if inbound, going forwards, so transfer FROM this current stop
            transfer_map_it = transfer_links_o_d_.find(current_label_stop.stop_id_);
            found_transfers = (transfer_map_it != transfer_links_o_d_.end());
        }
        if (!found_transfers) { return; }

        // Lookup transfer weights
        UserClassMode ucm = { path_spec.user_class_, MODE_TRANSFER, "transfer"};
        WeightLookup::const_iterator iter_wl = weight_lookup_.find(ucm);
        if (iter_wl == weight_lookup_.end()) { return; }
        SupplyModeToNamedWeights::const_iterator iter_sm2nw = iter_wl->second.find(transfer_supply_mode_);
        if (iter_sm2nw == iter_wl->second.end()) { return; }
        const NamedWeights& transfer_weights = iter_sm2nw->second;

        for (StopToAttr::const_iterator transfer_it = transfer_map_it->second.begin();
             transfer_it != transfer_map_it->second.end(); ++transfer_it)
        {
            int     xfer_stop_id    = transfer_it->first;
            double  transfer_time   = transfer_it->second.find("time_min")->second;
            // outbound: departure time = latest departure - transfer
            //  inbound: arrival time   = earliest arrival + transfer
            double  deparr_time     = latest_dep_earliest_arr - (transfer_time*dir_factor);
            double  link_cost, cost;

            // stochastic/hyperpath: cost update
            if (path_spec.hyperpath_)
            {
                Attributes link_attr            = transfer_it->second;
                link_attr["transfer_penalty"]   = 1.0;
                link_cost                       = tallyLinkCost(transfer_supply_mode_, path_spec, trace_file, transfer_weights, link_attr);
                cost                            = nonwalk_label + link_cost;

            }
            // deterministic: label = cost = total time, just additive
            else
            {
                link_cost           = transfer_time;
                cost                = current_label_stop.label_ + link_cost;

                // check (departure mode, stop) if someone's waiting already
                // curious... this only applies to OUTBOUND
                // TODO: capacity stuff
                if (path_spec.outbound_)
                {
                    TripStop ts = { current_trip, current_stop_state[0].seq_, current_label_stop.stop_id_ };
                    std::map<TripStop, double, struct TripStopCompare>::const_iterator bwi = bump_wait_.find(ts);
                    if (bwi != bump_wait_.end())
                    {
                        // time a bumped passenger started waiting
                        double latest_time = bwi->second;
                        // we can't come in time
                        if (deparr_time - TIME_WINDOW_ > latest_time) { continue; }
                        // leave earlier -- to get in line 5 minutes before bump wait time
                        // (confused... We don't resimulate previous bumping passenger so why does this make sense?)
                        cost            = cost + (current_stop_state[0].deparr_time_ - latest_time) + BUMP_BUFFER_;
                        deparr_time     = latest_time - transfer_time - BUMP_BUFFER_;
                    }
                }
            }

            // addStopState will handle logic of updating total cost
            StopState ss = {
                deparr_time,                    // departure/arrival time
                MODE_TRANSFER,                  // departure/arrival mode
                1 ,                             // trip id
                current_label_stop.stop_id_,    // successor/predecessor
                -1,                             // sequence
                -1,                             // sequence succ/pred
                transfer_time,                  // link time
                link_cost,                      // link cost
                cost,                           // cost
                label_iteration,                // label iteration
                latest_dep_earliest_arr         // arrival/departure time
            };
            addStopState(path_spec, trace_file, xfer_stop_id, ss, stop_states, label_stop_queue, hyperpath_ss);
        }
    }

    void PathFinder::updateStopStatesForTrips(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        StopStates& stop_states,
        LabelStopQueue& label_stop_queue,
        HyperpathStopStates& hyperpath_ss,
        int label_iteration,
        const LabelStop& current_label_stop,
        std::tr1::unordered_set<int>& trips_done) const
    {
        double dir_factor = path_spec.outbound_ ? 1.0 : -1.0;

        // for weight lookup
        UserClassMode ucm = { path_spec.user_class_, MODE_TRANSIT, path_spec.transit_mode_};
        WeightLookup::const_iterator iter_weights = weight_lookup_.find(ucm);
        if (iter_weights == weight_lookup_.end()) {
            return;
        }

        // current_stop_state is a vector
        std::vector<StopState>& current_stop_state = stop_states[current_label_stop.stop_id_];
        int current_mode = current_stop_state[0].deparr_mode_;      // why index 0 for hyperpath?-- should this be the latest_dep_earliest_arrival mode?
        int current_trip_id = current_stop_state[0].trip_id_;
        double latest_dep_earliest_arr = current_stop_state[0].deparr_time_;
        if (path_spec.hyperpath_) {
            latest_dep_earliest_arr = hyperpath_ss[current_label_stop.stop_id_].latest_dep_earliest_arr_;
        }

        // Update by trips
        std::vector<TripStopTime> relevant_trips;
        getTripsWithinTime(current_label_stop.stop_id_, path_spec.outbound_, latest_dep_earliest_arr, relevant_trips);
        for (std::vector<TripStopTime>::const_iterator it=relevant_trips.begin(); it != relevant_trips.end(); ++it) {

            // don't include the trip that's determining the time boundary -- we don't want to just use that again
            // otherwise it is likely to end up the best one and then we'll end up having no other option but to choose two links in a row from the same trip
            if (path_spec.hyperpath_ && hyperpath_ss[current_label_stop.stop_id_].lder_trip_id_ == it->trip_id_) { continue; }

            // the trip info for this trip
            const TripInfo& trip_info = trip_info_.find(it->trip_id_)->second;

            // get the weights applicable for this trip
            SupplyModeToNamedWeights::const_iterator iter_sm2nw = iter_weights->second.find(trip_info.supply_mode_num_);
            if (iter_sm2nw == iter_weights->second.end()) {
                // this supply mode isn't allowed for the userclass/demand mode
                continue;
            }
            const NamedWeights& named_weights = iter_sm2nw->second;

            if (true && path_spec.trace_) {
                trace_file << "valid trips: " << trip_num_to_str_.find(it->trip_id_)->second << " " << it->seq_ << " ";
                printTime(trace_file, path_spec.outbound_ ? it->arrive_time_ : it->depart_time_);
                trace_file << std::endl;
            }

            // trip is already processed
            // if (trips_done.find(it->trip_id_) != trips_done.end()) continue;

            // trip arrival time (outbound) / trip departure time (inbound)
            double arrdep_time = path_spec.outbound_ ? it->arrive_time_ : it->depart_time_;
            double wait_time = (latest_dep_earliest_arr - arrdep_time)*dir_factor;
            double arrive_time;
            if (wait_time < 0) {
                printf("wait_time < 0 -- this shouldn't happen!\n");
                if (path_spec.trace_) { trace_file << "wait_time < 0 -- this shouldn't happen!" << std::endl; }
            }

            // deterministic path-finding: check capacities
            if (!path_spec.hyperpath_) {
                TripStop check_for_bump_wait;
                if (path_spec.outbound_) {
                    // if outbound, this trip loop is possible trips *before* the current trip
                    // checking that we get here in time for the current trip
                    check_for_bump_wait.trip_id_ = current_stop_state[0].trip_id_;
                    check_for_bump_wait.seq_     = current_stop_state[0].seq_;
                    check_for_bump_wait.stop_id_ = current_label_stop.stop_id_;
                    //  arrive from the loop trip
                    arrive_time = arrdep_time;
                } else {
                    // if inbound, the trip is the next trip
                    // checking that we can get here in time for that trip
                    check_for_bump_wait.trip_id_ = it->trip_id_;
                    check_for_bump_wait.seq_     = it->seq_;
                    check_for_bump_wait.stop_id_ = current_label_stop.stop_id_;
                    // arrive for this trip
                    arrive_time = current_stop_state[0].deparr_time_;
                }
                std::map<TripStop, double, struct TripStopCompare>::const_iterator bwi = bump_wait_.find(check_for_bump_wait);
                if (bwi != bump_wait_.end()) {
                    // time a bumped passenger started waiting
                    float latest_time = bwi->second;
                    if (path_spec.trace_) {
                        trace_file << "checking latest_time ";
                        printTime(trace_file, latest_time);
                        trace_file << " vs arrive_time ";
                        printTime(trace_file, arrive_time);
                        trace_file << " for potential trip " << it->trip_id_ << std::endl;
                    }
                    if ((arrive_time + 0.01 >= latest_time) &&
                        (current_stop_state[0].trip_id_ != it->trip_id_)) {
                        if (path_spec.trace_) { trace_file << "Continuing" << std::endl; }
                        continue;
                    }
                }
            }

            // get the TripStopTimes for this trip
            std::map<int, std::vector<TripStopTime> >::const_iterator tstiter = trip_stop_times_.find(it->trip_id_);
            assert(tstiter != trip_stop_times_.end());
            const std::vector<TripStopTime>& possible_stops = tstiter->second;

            // these are the relevant potential trips/stops; iterate through them
            unsigned int start_seq = path_spec.outbound_ ? 1 : it->seq_+1;
            unsigned int end_seq   = path_spec.outbound_ ? it->seq_-1 : possible_stops.size();
            for (unsigned int seq_num = start_seq; seq_num <= end_seq; ++seq_num) {
                // possible board for outbound / alight for inbound
                const TripStopTime& possible_board_alight = possible_stops.at(seq_num-1);

                // new label = length of trip so far if the passenger boards/alights at this stop
                int board_alight_stop = possible_board_alight.stop_id_;
                StopStates::const_iterator possible_stop_state_iter = stop_states.find(board_alight_stop);

                // hyperpath: potential successor/predessor can't be access or egress
                if (path_spec.hyperpath_) {
                    if (possible_stop_state_iter != stop_states.end() && possible_stop_state_iter->second.size()>0) {
                        int possible_mode = possible_stop_state_iter->second.front().deparr_mode_; // first mode; why 0 index?
                        if ((possible_mode == MODE_ACCESS) || (possible_mode == MODE_EGRESS)) { continue; }
                    }
                }

                double  deparr_time     = path_spec.outbound_ ? possible_board_alight.depart_time_ : possible_board_alight.arrive_time_;
                // the schedule crossed midnight
                if (path_spec.outbound_ && arrdep_time < deparr_time) {
                    deparr_time -= 24*60;
                    if (path_spec.trace_) { trace_file << "trip crossed midnight; adjusting deparr_time" << std::endl; }
                } else if (!path_spec.outbound_ && deparr_time < arrdep_time) {
                    deparr_time += 24*60;
                    if (path_spec.trace_) { trace_file << "trip crossed midnight; adjusting deparr_time" << std::endl; }
                }
                double  in_vehicle_time = (arrdep_time - deparr_time)*dir_factor;
                double  cost      = 0;
                double  link_cost = 0;

                if (in_vehicle_time < 0) {
                    printf("in_vehicle_time < 0 -- this shouldn't happen\n");
                    if (path_spec.trace_) { trace_file << "in_vehicle_time < 0 -- this shouldn't happen!" << std::endl; }
                }

                // stochastic/hyperpath: cost update
                if (path_spec.hyperpath_) {

                    // start with trip info attributes
                    Attributes link_attr = trip_info.trip_attr_;
                    link_attr["in_vehicle_time_min"] = in_vehicle_time;
                    link_attr["wait_time_min"      ] = wait_time;

                    link_cost = 0;
                    // If outbound, and the current link is egress, then it's as late as possible and the wait time isn't accurate.
                    // It should be a preferred delay time instead
                    // ditto for inbound and access
                    if (( path_spec.outbound_ && current_mode == MODE_EGRESS) ||
                        (!path_spec.outbound_ && current_mode == MODE_ACCESS)) {
                        link_attr["wait_time_min"      ] = 0;


                        // TODO: this is awkward... setting this all up again.  Plus we don't have all the attributes set.  Cache something?
                        Attributes delay_attr;
                        delay_attr["time_min"           ] = 0;
                        delay_attr["preferred_delay_min"] = wait_time;
                        UserClassMode delay_ucm = { path_spec.user_class_,
                                                    path_spec.outbound_ ? MODE_EGRESS: MODE_ACCESS,
                                                    path_spec.outbound_ ? path_spec.egress_mode_ : path_spec.access_mode_
                                                  };
                        WeightLookup::const_iterator delay_iter_weights = weight_lookup_.find(delay_ucm);
                        if (delay_iter_weights != weight_lookup_.end()) {
                            SupplyModeToNamedWeights::const_iterator delay_iter_s2w = delay_iter_weights->second.find(current_trip_id);
                            if (delay_iter_s2w != delay_iter_weights->second.end()) {
                                link_cost = tallyLinkCost(current_trip_id, path_spec, trace_file, delay_iter_s2w->second, delay_attr);
                            }
                        }
                    }

                    // if we have a zero walk transfer, we still need to penalize
                    else if (isTrip(current_mode)) {
                        // TODO: this is awkward... setting this all up again.  Plus we don't have all the attributes set.  Cache something?
                        Attributes xfer_attr;
                        xfer_attr["transfer_penalty"] = 1.0;
                        xfer_attr["walk_time_min"   ] = 0.0;

                        UserClassMode xfer_ucm = { path_spec.user_class_, MODE_TRANSFER, "transfer"};
                        WeightLookup::const_iterator xfer_iter_weights = weight_lookup_.find(xfer_ucm);
                        if (xfer_iter_weights != weight_lookup_.end()) {
                           SupplyModeToNamedWeights::const_iterator xfer_iter_s2w = xfer_iter_weights->second.find(transfer_supply_mode_);
                           if (xfer_iter_s2w != xfer_iter_weights->second.end()) {
                                link_cost = tallyLinkCost(transfer_supply_mode_, path_spec, trace_file, xfer_iter_s2w->second, xfer_attr);
                           }
                        }
                    }

                    // if we calculate the transfer penalty on the transit links.
                    // I think we can't do this as it's problematic
                    // TODO: devise test to demonstrate
                    // these are special -- set them
                    if ((current_mode == MODE_ACCESS) || (current_mode == MODE_EGRESS)) {
                        link_attr["transfer_penalty"] = 0.0;
                    } else {
                        link_attr["transfer_penalty"] = 1.0;
                    }

                    // if we have a zero walk transfer, we still need to penalize
                    if (isTrip(current_mode)) {
                        // TODO: this is awkward... setting this all up again.  Plus we don't have all the attributes set.  Cache something?
                        Attributes xfer_attr;
                        xfer_attr["transfer_penalty"] = 1.0;
                        xfer_attr["walk_time_min"   ] = 0.0;

                        UserClassMode xfer_ucm = { path_spec.user_class_, MODE_TRANSFER, "transfer"};
                        WeightLookup::const_iterator xfer_iter_weights = weight_lookup_.find(xfer_ucm);
                        if (xfer_iter_weights != weight_lookup_.end()) {
                           SupplyModeToNamedWeights::const_iterator xfer_iter_s2w = xfer_iter_weights->second.find(transfer_supply_mode_);
                           if (xfer_iter_s2w != xfer_iter_weights->second.end()) {
                                link_cost = tallyLinkCost(transfer_supply_mode_, path_spec, trace_file, xfer_iter_s2w->second, xfer_attr);
                           }
                        }
                    }

                    link_cost = link_cost + tallyLinkCost(trip_info.supply_mode_num_, path_spec, trace_file, named_weights, link_attr);
                    cost      = hyperpath_ss[current_label_stop.stop_id_].hyperpath_cost_ + link_cost;

                }
                // deterministic: label = cost = total time, just additive
                else {
                    link_cost   = in_vehicle_time + wait_time;
                    cost        = current_stop_state[0].cost_ + link_cost;
                }

                StopState ss = {
                    deparr_time,                    // departure/arrival time
                    MODE_TRANSIT,                   // departure/arrival mode
                    possible_board_alight.trip_id_, // trip id
                    current_label_stop.stop_id_,    // successor/predecessor
                    possible_board_alight.seq_,     // sequence
                    it->seq_,                       // sequence succ/pred
                    in_vehicle_time+wait_time,      // link time
                    link_cost,                      // link cost
                    cost,                           // cost
                    label_iteration,                // label iteration
                    arrdep_time                     // arrival/departure time
                };
                addStopState(path_spec, trace_file, board_alight_stop, ss, stop_states, label_stop_queue, hyperpath_ss);

            }
            trips_done.insert(it->trip_id_);
        }
    }

    int PathFinder::labelStops(const PathSpecification& path_spec,
                                          std::ofstream& trace_file,
                                          StopStates& stop_states,
                                          LabelStopQueue& label_stop_queue,
                                          HyperpathStopStates& hyperpath_ss,
                                          int& max_process_count) const
    {
        int label_iterations = 1;
        std::tr1::unordered_set<int> stop_done;
        std::tr1::unordered_set<int> trips_done;
        double dir_factor = path_spec.outbound_ ? 1.0 : -1.0;
        LabelStop last_label_stop;

        while (!label_stop_queue.empty()) {
            /***************************************************************************************
            * for outbound: we can depart from *stop_id*
            *                      via *departure mode*
            *                      at *departure time*
            *                      and get to stop *successor*
            *                      and the total cost from *stop_id* to the destination TAZ is *label*
            * for inbound: we can arrive at *stop_id*
            *                     via *arrival mode*
            *                     at *arrival time*
            *                     from stop *predecessor*
            *                     and the total cost from the origin TAZ to the *stop_id* is *label*
            **************************************************************************************/
            LabelStop current_label_stop = label_stop_queue.pop_top(stop_num_to_str_, path_spec.trace_, trace_file);

            // if we just processed this one, then skip since it'll be a no-op
            if (current_label_stop.stop_id_ == last_label_stop.stop_id_) { continue; }

            // hyperpath only
            if (path_spec.hyperpath_) {
                // have we hit the configured limit?
                if ((STOCH_MAX_STOP_PROCESS_COUNT_ > 0) && (hyperpath_ss[current_label_stop.stop_id_].process_count_ == STOCH_MAX_STOP_PROCESS_COUNT_)) {
                    if (path_spec.trace_) {
                        trace_file << "Pulling from label_stop_queue but stop " << stop_num_to_str_.find(current_label_stop.stop_id_)->second;
                        trace_file << " has been processed the limit " << STOCH_MAX_STOP_PROCESS_COUNT_ << " times so skipping." << std::endl;
                    }
                    continue;
                }
                // stop is processing
                hyperpath_ss[current_label_stop.stop_id_].process_count_ += 1;
                max_process_count = std::max(max_process_count, hyperpath_ss[current_label_stop.stop_id_].process_count_);
            }

            // no transfers to the stop
            // todo? continue if there are no transfers to/from the stop?

            // current_stop_state is a vector
            std::vector<StopState>& current_stop_state = stop_states[current_label_stop.stop_id_];

            if (path_spec.trace_) {
                trace_file << "Pulling from label_stop_queue (iteration " << std::setw( 6) << std::setfill(' ') << label_iterations;
                trace_file << ", stop " << stop_num_to_str_.find(current_label_stop.stop_id_)->second;
                if (path_spec.hyperpath_) {
                    trace_file << ", count " << hyperpath_ss[current_label_stop.stop_id_].process_count_;
                    trace_file << ", label ";
                    trace_file << std::setprecision(6) << current_label_stop.label_;
                }
                trace_file << ", cost ";
                if (path_spec.hyperpath_) {
                    trace_file << std::setprecision(6) << hyperpath_ss[current_label_stop.stop_id_].hyperpath_cost_;
                }
                else {
                    printTimeDuration(trace_file, current_stop_state[0].cost_);
                }
                trace_file << ", len "  << current_stop_state.size();
                if (path_spec.hyperpath_) {
                    trace_file << (path_spec.outbound_ ? ", latest_dep " : ", earliest_arr ");
                    printTime(trace_file, hyperpath_ss[current_label_stop.stop_id_].latest_dep_earliest_arr_);
                }
                trace_file << ") :======" << std::endl;
                trace_file << "        ";
                printStopStateHeader(trace_file, path_spec);
                trace_file << std::endl;
                for (std::vector<StopState>::const_iterator ssi  = current_stop_state.begin();
                                                            ssi != current_stop_state.end(); ++ssi) {
                    trace_file << "        ";
                    printStopState(trace_file, current_label_stop.stop_id_, *ssi, path_spec);
                    trace_file << std::endl;
                }
                trace_file << "==============================" << std::endl;

                stopids_file << stop_num_to_str_.find(current_label_stop.stop_id_)->second << "," << label_iterations << std::endl;
            }

            updateStopStatesForTransfers(path_spec,
                                         trace_file,
                                         stop_states,
                                         label_stop_queue,
                                         hyperpath_ss,
                                         label_iterations,
                                         current_label_stop);

            updateStopStatesForTrips(path_spec,
                                     trace_file,
                                     stop_states,
                                     label_stop_queue,
                                     hyperpath_ss,
                                     label_iterations,
                                     current_label_stop,
                                     trips_done);

            //  Done with this label iteration!
            label_iterations += 1;

            last_label_stop = current_label_stop;
        }
        return label_iterations;
    }


    bool PathFinder::finalizeTazState(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        StopStates& stop_states,
        LabelStopQueue& label_stop_queue,
        int label_iteration,
        HyperpathStopStates& hyperpath_ss) const
    {
        int end_taz_id = path_spec.outbound_ ? path_spec.origin_taz_id_ : path_spec.destination_taz_id_;
        double dir_factor = path_spec.outbound_ ? 1.0 : -1.0;

        // instantiate this
        std::vector<StopState>& taz_state = stop_states[end_taz_id];

        // are there any egress/access links?
        TAZSupplyStopToAttr::const_iterator iter_tss2a = taz_access_links_.find(end_taz_id);
        if (iter_tss2a == taz_access_links_.end()) {
            return false;
        }

        // Are there any supply modes for this demand mode?
        UserClassMode ucm = { path_spec.user_class_,
                              path_spec.outbound_ ? MODE_ACCESS: MODE_EGRESS,
                              path_spec.outbound_ ? path_spec.access_mode_ : path_spec.egress_mode_
                            };
        WeightLookup::const_iterator iter_weights = weight_lookup_.find(ucm);
        if (iter_weights == weight_lookup_.end()) {
            std::cerr << "Couldn't find any weights configured for user class [" << path_spec.user_class_ << "], ";
            std::cerr << (path_spec.outbound_ ? "egress mode [" : "access mode [");
            std::cerr << (path_spec.outbound_ ? path_spec.egress_mode_ : path_spec.access_mode_) << "]" << std::endl;
            return false;
        }

        if (path_spec.trace_) {
            stopids_file << stop_num_to_str_.find(end_taz_id)->second << "," << label_iteration << std::endl;
        }

        // Iterate through valid supply modes
        SupplyModeToNamedWeights::const_iterator iter_s2w;
        for (iter_s2w  = iter_weights->second.begin();
             iter_s2w != iter_weights->second.end(); ++iter_s2w) {
            int supply_mode_num = iter_s2w->first;

            if (path_spec.trace_) {
                trace_file << "Weights exist for supply mode " << supply_mode_num << " => ";
                trace_file << mode_num_to_str_.find(supply_mode_num)->second << std::endl;
            }

            // Are there any egress/access links for the supply mode?
            SupplyStopToAttr::const_iterator iter_ss2a = iter_tss2a->second.find(supply_mode_num);
            if (iter_ss2a == iter_tss2a->second.end()) {
                if (path_spec.trace_) {
                    trace_file << "No links for this supply mode" << std::endl;
                }
                continue;
            }

            // Iterate through the links for the given supply mode
            StopToAttr::const_iterator link_iter;
            for (link_iter  = iter_ss2a->second.begin();
                 link_iter != iter_ss2a->second.end(); ++link_iter)
            {

                int     stop_id                 = link_iter->first;
                Attributes link_attr            = link_iter->second;
                link_attr["preferred_delay_min"]= 0.0;

                double  access_time             = link_attr.find("time_min")->second;

                double  earliest_dep_latest_arr = PathFinder::MAX_DATETIME;
                double  nonwalk_label           = PathFinder::MAX_COST;

                bool    use_new_state           = false;
                double  deparr_time, link_cost, cost;

                std::map<int, std::vector<StopState> >::const_iterator stop_states_iter = stop_states.find(stop_id);
                if (stop_states_iter == stop_states.end()) { continue; }

                const std::vector<StopState>& current_stop_state = stop_states_iter->second;
                earliest_dep_latest_arr = current_stop_state[0].deparr_time_;

                if (path_spec.hyperpath_)
                {
                    for (std::vector<StopState>::const_iterator ssi  = current_stop_state.begin();
                                                                ssi != current_stop_state.end(); ++ssi)
                    {
                        if (path_spec.outbound_) {
                            earliest_dep_latest_arr = std::min(earliest_dep_latest_arr, ssi->deparr_time_);
                        } else {
                            earliest_dep_latest_arr = std::max(earliest_dep_latest_arr, ssi->deparr_time_);
                        }
                    }
                    nonwalk_label = calculateNonwalkLabel(current_stop_state);
                    // if nonwalk label == MAX_COST then the only way to reach this stop is via transfer so we don't want to walk again
                    if (nonwalk_label == PathFinder::MAX_COST) continue;

                    deparr_time = earliest_dep_latest_arr - (access_time*dir_factor);

                    link_cost       = tallyLinkCost(supply_mode_num, path_spec, trace_file, iter_s2w->second, link_attr);
                    cost            = nonwalk_label + link_cost;

                }
                // deterministic
                else
                {
                    deparr_time = earliest_dep_latest_arr - (access_time*dir_factor);

                    // first leg has to be a trip
                    if (current_stop_state.front().deparr_mode_ == MODE_TRANSFER) { continue; }
                    if (current_stop_state.front().deparr_mode_ == MODE_EGRESS  ) { continue; }
                    if (current_stop_state.front().deparr_mode_ == MODE_ACCESS  ) { continue; }
                    link_cost = access_time;
                    cost      = current_stop_state.front().cost_ + link_cost;

                    // capacity check
                    if (path_spec.outbound_)
                    {
                        TripStop ts = { current_stop_state[0].deparr_mode_, current_stop_state[0].seq_, stop_id };
                        std::map<TripStop, double, struct TripStopCompare>::const_iterator bwi = bump_wait_.find(ts);
                        if (bwi != bump_wait_.end()) {
                            // time a bumped passenger started waiting
                            float latest_time = bwi->second;
                            // we can't come in time
                            if (deparr_time - TIME_WINDOW_ > latest_time) { continue; }
                            // leave earlier -- to get in line 5 minutes before bump wait time
                            cost   = cost + (current_stop_state[0].deparr_time_ - latest_time) + BUMP_BUFFER_;
                            deparr_time = latest_time - access_time - BUMP_BUFFER_;
                        }
                    }

                }

                StopState ts = {
                    deparr_time,                                                                // departure/arrival time
                    path_spec.outbound_ ? MODE_ACCESS : MODE_EGRESS,                            // departure/arrival mode
                    supply_mode_num,                                                            // trip id
                    stop_id,                                                                    // successor/predecessor
                    -1,                                                                         // sequence
                    -1,                                                                         // sequence succ/pred
                    access_time,                                                                // link time
                    link_cost,                                                                  // link cost
                    cost,                                                                       // cost
                    label_iteration,                                                            // label iteration
                    earliest_dep_latest_arr                                                     // arrival/departure time
                };
                addStopState(path_spec, trace_file, end_taz_id, ts, stop_states, label_stop_queue, hyperpath_ss);

            } // end iteration through links for the given supply mode
        } // end iteration through valid supply modes
    }


    bool PathFinder::hyperpathGeneratePath(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        const StopStates& stop_states,
        const HyperpathStopStates& hyperpath_ss,
        Path& path) const
    {
        int    start_state_id   = path_spec.outbound_ ? path_spec.origin_taz_id_ : path_spec.destination_taz_id_;
        double dir_factor       = path_spec.outbound_ ? 1 : -1;
        
        const std::vector<StopState>& taz_state = stop_states.find(start_state_id)->second;
        double taz_label        = hyperpath_ss.find(start_state_id)->second.hyperpath_cost_;
        int    cost_cutoff      = 1;

        // setup access/egress probabilities
        std::vector<ProbabilityStop> access_cum_prob; // access/egress cumulative probabilities
        for (size_t state_index = 0; state_index < taz_state.size(); ++state_index)
        {
            double probability = exp(-1.0*STOCH_DISPERSION_*taz_state[state_index].cost_) /
                                 exp(-1.0*STOCH_DISPERSION_*taz_label);
            // why?  :p
            int prob_i = static_cast<int>(RAND_MAX*probability);
            // too small to consider
            if (prob_i < cost_cutoff) { continue; }
            if (access_cum_prob.size() == 0) {
                ProbabilityStop pb = { probability, prob_i, taz_state[state_index].stop_succpred_, state_index };
                access_cum_prob.push_back( pb );
            } else {
                ProbabilityStop pb = { probability, access_cum_prob.back().prob_i_ + prob_i, taz_state[state_index].stop_succpred_, state_index };
                access_cum_prob.push_back( pb );
            }
            if (path_spec.trace_) {
                printStopState(trace_file, start_state_id, taz_state[state_index], path_spec);
                trace_file << " : prob ";
                trace_file << std::setw(10) << probability << " cum_prob ";
                trace_file << std::setw( 6) << access_cum_prob.back().prob_i_;
                trace_file << "; index " << access_cum_prob.back().index_ << std::endl;
            }
        }

        size_t chosen_index = chooseState(path_spec, trace_file, access_cum_prob);
        StopState ss = taz_state[chosen_index];
        path.push_back( std::make_pair(start_state_id, ss) );

        if (path_spec.trace_)
        {
            trace_file << " -> Chose access/egress ";
            printStopState(trace_file, start_state_id, ss, path_spec);
            trace_file << std::endl;
        }

        int     current_stop_id = ss.stop_succpred_;
        // outbound: arrival time
        //  inbound: departure time
        double  arrdep_time     = ss.deparr_time_ + (ss.link_time_*dir_factor);
        int     prev_mode       = ss.deparr_mode_;
        int     prev_trip_id    = ss.trip_id_;
        while (true)
        {
            // setup probabilities
            if (path_spec.trace_) {
                trace_file << "current_stop=" << stop_num_to_str_.find(current_stop_id)->second;
                trace_file << (path_spec.outbound_ ? "; arrival_time=" : "; departure_time=");
                printTime(trace_file, arrdep_time);
                trace_file << "; prev_mode=";
                printMode(trace_file, prev_mode, prev_trip_id);
                trace_file << std::endl;
                trace_file << "            ";
                printStopStateHeader(trace_file, path_spec);
                trace_file << std::endl;
            }
            std::vector<ProbabilityStop> stop_cum_prob;
            double sum_exp = 0;
            StopStates::const_iterator ssi = stop_states.find(current_stop_id);
            for (size_t stop_state_index = 0; stop_state_index < ssi->second.size(); ++stop_state_index)
            {
                const StopState& state = ssi->second[stop_state_index];

                // no repeat of access/egress
                if ( path_spec.outbound_ && state.deparr_mode_ == MODE_ACCESS) { continue; }
                if (!path_spec.outbound_ && state.deparr_mode_ == MODE_EGRESS) { continue; }
                // no double walk
                if (path_spec.outbound_ &&
                    ((state.deparr_mode_ == MODE_EGRESS) || (state.deparr_mode_ == MODE_TRANSFER)) &&
                    ((         prev_mode == MODE_ACCESS) || (         prev_mode == MODE_TRANSFER))) { continue; }
                if (!path_spec.outbound_ &&
                    ((state.deparr_mode_ == MODE_ACCESS) || (state.deparr_mode_ == MODE_TRANSFER)) &&
                    ((         prev_mode == MODE_EGRESS) || (         prev_mode == MODE_TRANSFER))) { continue; }
                // don't double on the same trip ID - that's already covered by a single trip
                if (state.deparr_mode_ == MODE_TRANSIT && state.trip_id_ == prev_trip_id) { continue; }

                // outbound: we cannot depart before we arrive
                if (path_spec.outbound_ && state.deparr_time_ < arrdep_time) { continue; }
                // inbound: we cannot arrive after we depart
                if (!path_spec.outbound_ && state.deparr_time_ > arrdep_time) { continue; }

                // calculating denominator
                sum_exp += exp(-1.0*STOCH_DISPERSION_*state.cost_);
                // probabilities will be filled in later - use cost for now
                ProbabilityStop pb = { state.cost_, 0, state.stop_succpred_, stop_state_index };
                stop_cum_prob.push_back(pb);

                if (path_spec.trace_) {
                    trace_file << "            ";
                    printStopState(trace_file, current_stop_id, state, path_spec);
                    trace_file << "  sum_exp = " << std::scientific << sum_exp << std::endl;
                }
            }

            // dead end
            if (stop_cum_prob.size() == 0) {
                return false;
            }
            if (sum_exp == 0) {
                return false;
            }

            // denom found - cum prob time
            for (size_t idx = 0; idx < stop_cum_prob.size(); ++idx) {
                double probability = exp(-1.0*STOCH_DISPERSION_*stop_cum_prob[idx].probability_) / sum_exp;

                // why?  :p
                int prob_i = static_cast<int>(RAND_MAX*probability);

                stop_cum_prob[idx].probability_ = probability;
                if (idx == 0) {
                    stop_cum_prob[idx].prob_i_ = prob_i;
                } else {
                    stop_cum_prob[idx].prob_i_ = prob_i + stop_cum_prob[idx-1].prob_i_;
                }
                if (path_spec.trace_) {
                    printStopState(trace_file, current_stop_id, ssi->second[stop_cum_prob[idx].index_], path_spec);
                    trace_file << std::setw( 6) << std::setfill(' ') << std::fixed << stop_cum_prob[idx].stop_id_ << " ";
                    trace_file << ": prob ";
                    trace_file << std::setw(10) << probability << " cum_prob ";
                    trace_file << std::setw( 6) << stop_cum_prob[idx].prob_i_ << std::endl;
                }
            }

            // choose!
            size_t chosen_index = chooseState(path_spec, trace_file, stop_cum_prob);
            StopState next_ss   = ssi->second[chosen_index];

            if (path_spec.trace_) {
                trace_file << " -> Chose stop link ";
                printStopState(trace_file, current_stop_id, next_ss, path_spec);
                trace_file << std::endl;
            }

            // UPDATES to states
            // Hyperpaths have some uncertainty built in which we need to rectify as we go through and choose
            // concrete path states.

            // OUTBOUND: We are choosing links in chronological order.
            if (path_spec.outbound_)
            {
                // Leave origin as late as possible
                if (prev_mode == MODE_ACCESS) {
                    double dep_time = getScheduledDeparture(next_ss.trip_id_, current_stop_id, next_ss.seq_);
                    // set departure time for the access link to perfectly catch the vehicle
                    // todo: what if there is a wait queue?
                    path.back().second.arrdep_time_ = dep_time;
                    path.back().second.deparr_time_ = dep_time - path.front().second.link_time_;
                    // no wait time for the trip
                    next_ss.link_time_ = next_ss.arrdep_time_ - next_ss.deparr_time_;
                }
                // *Fix trip time*
                else if (isTrip(next_ss.deparr_mode_)) {
                    // link time is arrival time - previous arrival time
                    next_ss.link_time_ = next_ss.arrdep_time_ - arrdep_time;
                }
                // *Fix transfer times*
                else if (next_ss.deparr_mode_ == MODE_TRANSFER) {
                    next_ss.deparr_time_ = path.back().second.arrdep_time_;   // start transferring immediately
                    next_ss.arrdep_time_ = next_ss.deparr_time_ + next_ss.link_time_;
                }
                // Egress: don't wait, just walk. Get to destination as early as possible
                else if (next_ss.deparr_mode_ == MODE_EGRESS) {
                    next_ss.deparr_time_ = path.back().second.arrdep_time_;
                    next_ss.arrdep_time_ = next_ss.deparr_time_ + next_ss.link_time_;
                }
            }
            // INBOUND: We are choosing links in REVERSE chronological order
            else
            {
                // Leave origin as late as possible
                if (next_ss.deparr_mode_ == MODE_ACCESS) {
                    double dep_time = getScheduledDeparture(path.back().second.trip_id_, current_stop_id, path.back().second.seq_succpred_);
                    // set arrival time for the access link to perfectly catch the vehicle
                    // todo: what if there is a wait queue?
                    next_ss.deparr_time_ = dep_time;
                    next_ss.arrdep_time_ = next_ss.deparr_time_ - next_ss.link_time_;
                    // no wait time for the trip
                    path.back().second.link_time_ = path.back().second.deparr_time_ - path.back().second.arrdep_time_;
                }
                // *Fix trip time*: we are choosing in reverse so pretend the wait time is zero for now to
                // accurately evaluate possible transfers in next choice.
                else if (isTrip(next_ss.deparr_mode_)) {
                    next_ss.link_time_ = next_ss.deparr_time_ - next_ss.arrdep_time_;
                    // If we just picked this trip and the previous (next in time) is transfer then we know the wait now
                    // and we can update the transfer and the trip with the real wait
                    if (prev_mode == MODE_TRANSFER) {
                        // move transfer time so we do it right after arriving
                        path.back().second.arrdep_time_ = next_ss.deparr_time_; // depart right away
                        path.back().second.deparr_time_ = next_ss.deparr_time_ + path.back().second.link_time_; // arrive after walk
                        // give the wait time to the previous trip
                        path[path.size()-2].second.link_time_ = path[path.size()-2].second.deparr_time_ - path.back().second.deparr_time_;
                    }
                    // If the previous (next in time) is another trip (so zero-walk transfer) give it wait time
                    else if (isTrip(prev_mode)) {
                        path.back().second.link_time_ = path.back().second.deparr_time_ - next_ss.deparr_time_;
                    }
                }
                // *Fix transfer depart/arrive times*: transfer as late as possible to preserve options for earlier trip
                else if (next_ss.deparr_mode_ == MODE_TRANSFER) {
                    next_ss.deparr_time_ = path.back().second.arrdep_time_;
                    next_ss.arrdep_time_ = next_ss.deparr_time_ - next_ss.link_time_;
                }
                // Egress: don't wait, just walk. Get to destination as early as possible
                if (prev_mode == MODE_EGRESS) {
                    path.back().second.arrdep_time_ = next_ss.deparr_time_;
                    path.back().second.deparr_time_ = path.back().second.arrdep_time_ + path.back().second.link_time_;
                }
            }


            // record the choice
            path.push_back( std::make_pair(current_stop_id, next_ss) );

            // move on to the next
            current_stop_id     = next_ss.stop_succpred_;
            prev_mode           = next_ss.deparr_mode_;
            prev_trip_id        = next_ss.trip_id_;

            // update arrival / departure time
            arrdep_time = next_ss.arrdep_time_;

            if (path_spec.trace_) {
                trace_file << " ->    Updated link ";
                printStopState(trace_file, path.back().first, path.back().second, path_spec);
                trace_file << std::endl;
            }

            // are we done?
            if (( path_spec.outbound_ && next_ss.deparr_mode_ == MODE_EGRESS) ||
                (!path_spec.outbound_ && next_ss.deparr_mode_ == MODE_ACCESS)) {
                break;
            }

        }
        return true;
    }

    Path PathFinder::choosePath(const PathSpecification& path_spec,
        std::ofstream& trace_file,
        PathSet& paths,
        int max_prob_i) const
    {
        int random_num = rand();
        if (path_spec.trace_) { trace_file << "random_num " << random_num << " -> "; }

        // mod it by max prob
        random_num = random_num % max_prob_i;
        if (path_spec.trace_) { trace_file << random_num << std::endl; }

        for (PathSet::const_iterator psi = paths.begin(); psi != paths.end(); ++psi)
        {
            if (psi->second.prob_i_==0) { continue; }
            if (random_num <= psi->second.prob_i_) { return psi->first; }
        }
        // shouldn't get here
        printf("PathFinder::choosePath() This should never happen!\n");
    }

    size_t PathFinder::chooseState(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        const std::vector<ProbabilityStop>& prob_stops) const
    {
        int random_num = rand();
        if (path_spec.trace_) { trace_file << "random_num " << random_num << " -> "; }

        // mod it by max prob
        random_num = random_num % (prob_stops.back().prob_i_);
        if (path_spec.trace_) { trace_file << random_num << std::endl; }

        for (size_t ind = 0; ind < prob_stops.size(); ++ind)
        {
            if (prob_stops[ind].prob_i_==0) { continue; }
            if (random_num <= prob_stops[ind].prob_i_) { return prob_stops[ind].index_; }
        }
        // shouldn't get here
        printf("PathFinder::chooseState() This should never happen!\n");
    }

    /**
     * Calculate the path cost now that we know all the links.  This may result in different
     * costs than the original costs.  This updates the path's StopState.cost_ attributes
     * as well as the PathInfo.cost_ attribute.
     */
    void PathFinder::calculatePathCost(const PathSpecification& path_spec,
        std::ofstream& trace_file,
        Path& path,
        PathInfo& path_info) const
    {
        // no stops - nothing to do
        if (path.size()==0) { return; }

        if (path_spec.trace_) {
            trace_file << "calculatePathCost:" << std::endl;
            printPath(trace_file, path_spec, path);
            trace_file << std::endl;
        }

        bool   first_trip           = true;
        double dir_factor           = path_spec.outbound_ ? 1.0 : -1.0;

        // iterate through the states in chronological order
        int start_ind       = path_spec.outbound_ ? 0 : path.size()-1;
        int end_ind         = path_spec.outbound_ ? path.size() : -1;
        int inc             = path_spec.outbound_ ? 1 : -1;

        path_info.cost_     = 0;
        for (int index = start_ind; index != end_ind; index += inc)
        {
            int stop_id             = path[index].first;
            StopState& stop_state   = path[index].second;

            // ============= access =============
            if (stop_state.deparr_mode_ == MODE_ACCESS)
            {
                // inbound: preferred time is origin departure time
                double orig_departure_time        = (path_spec.outbound_ ? stop_state.deparr_time_ : stop_state.deparr_time_ - stop_state.link_time_);
                double preference_delay           = (path_spec.outbound_ ? 0 : orig_departure_time - path_spec.preferred_time_);

                int transit_stop                  = (path_spec.outbound_ ? stop_state.stop_succpred_ : stop_id);
                UserClassMode ucm                 = { path_spec.user_class_, MODE_ACCESS, path_spec.access_mode_ };
                const NamedWeights& named_weights = weight_lookup_.find(ucm)->second.find(stop_state.trip_id_)->second;
                Attributes          attributes    = taz_access_links_.find(path_spec.origin_taz_id_)->second.find(stop_state.trip_id_)->second.find(transit_stop)->second;
                attributes["preferred_delay_min"] = preference_delay;

                stop_state.cost_                  = tallyLinkCost(stop_state.trip_id_, path_spec, trace_file, named_weights, attributes);
                path_info.cost_                  += stop_state.cost_;
            }
            // ============= egress =============
            else if (stop_state.deparr_mode_ == MODE_EGRESS)
            {
                // outbound: preferred time is destination arrival time
                double dest_arrival_time          = (path_spec.outbound_ ? stop_state.deparr_time_ + stop_state.link_time_ : stop_state.deparr_time_);
                double preference_delay           = (path_spec.outbound_ ? path_spec.preferred_time_ - dest_arrival_time : 0);

                int transit_stop                  = (path_spec.outbound_ ? stop_id : stop_state.stop_succpred_);
                UserClassMode ucm                 = { path_spec.user_class_, MODE_EGRESS, path_spec.egress_mode_ };
                const NamedWeights& named_weights = weight_lookup_.find(ucm)->second.find(stop_state.trip_id_)->second;
                Attributes          attributes    = taz_access_links_.find(path_spec.destination_taz_id_)->second.find(stop_state.trip_id_)->second.find(transit_stop)->second;
                attributes["preferred_delay_min"] = preference_delay;

                stop_state.cost_                  = tallyLinkCost(stop_state.trip_id_, path_spec, trace_file, named_weights, attributes);
                path_info.cost_                  += stop_state.cost_;

            }
            // ============= transfer =============
            else if (stop_state.deparr_mode_ == MODE_TRANSFER)
            {
                int orig_stop                     = (path_spec.outbound_? stop_id : stop_state.stop_succpred_);
                int dest_stop                     = (path_spec.outbound_? stop_state.stop_succpred_ : stop_id);

                Attributes link_attr;
                if (orig_stop != dest_stop) {
                    link_attr = transfer_links_o_d_.find(orig_stop)->second.find(dest_stop)->second;
                } else {
                    // TODO: this is awkward... Plus we don't nec have all the attributes set.  Store a default no-walk transfer link?
                    link_attr["walk_time_min"]    = 0.0;
                }
                link_attr["transfer_penalty"]     = 1.0;

                UserClassMode ucm                 = { path_spec.user_class_, MODE_TRANSFER, "transfer" };
                const NamedWeights& named_weights = weight_lookup_.find(ucm)->second.find(transfer_supply_mode_)->second;
                stop_state.cost_                  = tallyLinkCost(transfer_supply_mode_, path_spec, trace_file, named_weights, link_attr);
                path_info.cost_                  += stop_state.cost_;
            }
            // ============= trip =============
            else
            {
                double trip_ivt_min               = (stop_state.arrdep_time_ - stop_state.deparr_time_)*dir_factor;
                double wait_min                   = stop_state.link_time_ - trip_ivt_min;

                UserClassMode ucm                 = { path_spec.user_class_, MODE_TRANSIT, path_spec.transit_mode_ };
                const TripInfo& trip_info         = trip_info_.find(stop_state.trip_id_)->second;
                int supply_mode_num               = trip_info.supply_mode_num_;
                const NamedWeights& named_weights = weight_lookup_.find(ucm)->second.find(supply_mode_num)->second;
                Attributes link_attr              = trip_info.trip_attr_;
                link_attr["in_vehicle_time_min"]  = trip_ivt_min;
                link_attr["wait_time_min"]        = wait_min;

                if (first_trip) {
                    link_attr["transfer_penalty"] = 0.0;
                } else {
                    link_attr["transfer_penalty"] = 1.0;
                }

                stop_state.cost_                  = tallyLinkCost(supply_mode_num, path_spec, trace_file, named_weights, link_attr);
                path_info.cost_                  += stop_state.cost_;

                first_trip = false;

                // if the next link is a trip, insert a transfer link
                if (isTrip(path[index+inc].second.deparr_mode_)) {
                    int xfer_stop_id = path_spec.outbound_ ? stop_state.stop_succpred_ : stop_id;
                    StopState xfer_state = {
                        path_spec.outbound_ ? stop_state.arrdep_time_ : stop_state.deparr_time_, // departure/arrival time
                        MODE_TRANSFER,         // mode
                        transfer_supply_mode_, // trip id
                        xfer_stop_id,          // stop successor/pred
                        -1,                    // seq
                        -1,                    // seq successor/pred
                        0,                     // link time
                        0,                     // link cost  -- will be tallied later
                        stop_state.cost_,      // cost
                        -1,                    // iteration
                        path_spec.outbound_ ? stop_state.arrdep_time_ : stop_state.deparr_time_
                    };
                    if (path_spec.trace_) {
                        trace_file << "Adding ";
                        printStopState(trace_file, xfer_stop_id, xfer_state, path_spec);
                        trace_file << std::endl;
                    }
                    // going forward so insert after index, before index+1
                    if (path_spec.outbound_) {
                        path.insert( path.begin()+index+1, std::make_pair(xfer_stop_id, xfer_state) );
                        end_ind += 1;
                    }
                    // going backward so insert before index
                    else {
                        path.insert( path.begin()+index, std::make_pair(stop_id, xfer_state) );
                        index += 1;
                    }
                }
            }
        }

        if (path_spec.trace_) {
            trace_file << " ==================================================> cost: " << path_info.cost_ << std::endl;
            printPath(trace_file, path_spec, path);
            trace_file << std::endl;
        }
    }

    // Return success
    bool PathFinder::getFoundPath(
        const PathSpecification& path_spec,
        std::ofstream& trace_file,
        const StopStates& stop_states,
        const HyperpathStopStates& hyperpath_ss,
        Path& path,
        PathInfo& path_info) const
    {
        int end_taz_id = path_spec.outbound_ ? path_spec.origin_taz_id_ : path_spec.destination_taz_id_;

        // no taz states -> no path found
        const std::vector<StopState>& taz_state = stop_states.find(end_taz_id)->second;
        if (taz_state.size() == 0) { return false; }

        if (path_spec.hyperpath_)
        {
            // find a bunch!
            PathSet paths, paths_updated_cost;
            // random seed
            srand(path_spec.path_id_);
            // find a *set of Paths*
            for (int attempts = 1; attempts <= STOCH_PATHSET_SIZE_; ++attempts)
            {
                Path new_path;
                bool path_found = hyperpathGeneratePath(path_spec, trace_file, stop_states, hyperpath_ss, new_path);

                if (path_found) {
                    if (path_spec.trace_) {
                        trace_file << "----> Found path " << attempts << " ";
                        printPathCompat(trace_file, path_spec, new_path);
                        trace_file << std::endl;
                        printPath(trace_file, path_spec, new_path);
                        trace_file << std::endl;
                    }
                    // do we already have this?  if so, increment
                    PathSet::iterator paths_iter = paths.find(new_path);
                    if (paths_iter != paths.end()) {
                        paths_iter->second.count_ += 1;
                    } else {
                        PathInfo pi = { 1, 0, false, 0, 0 };  // count is 1
                        paths[new_path] = pi;
                    }
                    if (path_spec.trace_) { trace_file << "paths size = " << paths.size() << std::endl; }
                } else {
                    if (path_spec.trace_) {
                        trace_file << "----> No path found" << std::endl;
                    }
                }
            }
            // calculate the costs for those paths and the logsum
            double logsum = 0;
            for (PathSet::iterator paths_iter = paths.begin(); paths_iter != paths.end(); ++paths_iter)
            {
                // updated cost version
                Path     path_updated     = paths_iter->first;
                PathInfo pathinfo_updated = paths_iter->second;
                calculatePathCost(path_spec, trace_file, path_updated, pathinfo_updated);
                // save it into the new map
                paths_updated_cost[path_updated] = pathinfo_updated;
                if (pathinfo_updated.cost_ > 0)
                {
                    logsum += exp(-1.0*STOCH_DISPERSION_*pathinfo_updated.cost_);
                }
            }
            if (logsum == 0) { return false; } // fail

            // debug -- print pet set to file
            std::ofstream pathset_file;
            std::ostringstream ss;
            ss << output_dir_ << kPathSeparator;
            ss << "ft_pathset";
            if (process_num_ > 0) {
                ss << "_worker" << std::setfill('0') << std::setw(2) <<  process_num_;
            }
            ss << ".txt";
            // append
            pathset_file.open(ss.str().c_str(), (std::ios_base::out | std::ios_base::app));

            // for integerized probability*1000000
            int cum_prob    = 0;
            int cost_cutoff = 1;
            // calculate the probabilities for those paths
            for (PathSet::iterator paths_iter = paths_updated_cost.begin(); paths_iter != paths_updated_cost.end(); ++paths_iter)
            {
                paths_iter->second.probability_ = exp(-1.0*STOCH_DISPERSION_*paths_iter->second.cost_)/logsum;
                // why?  :p
                int prob_i = static_cast<int>(RAND_MAX*paths_iter->second.probability_);
                // too small to consider
                if (prob_i < cost_cutoff) { continue; }
                cum_prob += prob_i;
                paths_iter->second.prob_i_ = cum_prob;

                if (path_spec.trace_)
                {
                    trace_file << "-> probability " << std::setfill(' ') << std::setw(8) << paths_iter->second.probability_;
                    trace_file << "; prob_i " << std::setw(8) << paths_iter->second.prob_i_;
                    trace_file << "; count " << std::setw(4) << paths_iter->second.count_;
                    trace_file << "; cost " << std::setw(8) << paths_iter->second.cost_;
                    trace_file << "; cap bad? " << std::setw(2) << paths_iter->second.capacity_problem_;
                    trace_file << "   ";
                    printPathCompat(trace_file, path_spec, paths_iter->first);
                    trace_file << std::endl;
                }
                // print path to pathset file
                pathset_file << path_spec.iteration_ << " ";  // Iteration
                pathset_file << path_spec.passenger_id_ << " ";         // The passenger ID
                pathset_file << path_spec.path_id_ << " ";              // The path ID - uniquely identifies a passenger+path
                pathset_file << std::setw(8) << std::fixed << std::setprecision(2) << paths_iter->second.cost_ << " ";
                pathset_file << std::setw(8) << std::fixed << std::setprecision(6) << paths_iter->second.probability_ << " ";
                printPathCompat(pathset_file, path_spec, paths_iter->first);
                pathset_file << std::endl;
            }

            pathset_file.close();

            if (cum_prob == 0) { return false; } // fail

            // choose path
            path = choosePath(path_spec, trace_file, paths_updated_cost, cum_prob);
            path_info = paths_updated_cost[path];
        }
        else
        {
            // outbound: origin to destination
            // inbound:  destination to origin
            int final_state_type = path_spec.outbound_ ? MODE_EGRESS : MODE_ACCESS;

            StopState ss = taz_state.front(); // there's only one
            path.push_back( std::make_pair(end_taz_id, ss) );

            while (ss.deparr_mode_ != final_state_type)
            {
                int stop_id = ss.stop_succpred_;
                StopStates::const_iterator ssi = stop_states.find(stop_id);
                ss          = ssi->second.front();
                path.push_back( std::make_pair(stop_id, ss));

                int curr_index = path.size() - 1;
                int prev_index = curr_index - 1;

                if (path_spec.outbound_)
                {
                    // Leave origin as late as possible
                    if (path[prev_index].second.deparr_mode_ == MODE_ACCESS) {
                        path[prev_index].second.arrdep_time_ = ss.deparr_time_;
                        path[prev_index].second.deparr_time_ = path[prev_index].second.arrdep_time_ - path[prev_index].second.link_time_;
                        // no wait time for the trip
                        path[curr_index].second.link_time_   = path[curr_index].second.arrdep_time_ - path[curr_index].second.deparr_time_;
                    }
                    // *Fix trip time*
                    else if (isTrip(path[curr_index].second.deparr_mode_)) {
                        // link time is arrival time - previous arrival time
                        path[curr_index].second.link_time_ = path[curr_index].second.arrdep_time_ - path[prev_index].second.arrdep_time_;
                    }
                    // *Fix transfer times*
                    else if (path[curr_index].second.deparr_mode_ == MODE_TRANSFER) {
                        path[curr_index].second.deparr_time_ = path[prev_index].second.arrdep_time_;   // start transferring immediately
                        path[curr_index].second.arrdep_time_ = path[curr_index].second.deparr_time_ + path[curr_index].second.link_time_;
                    }
                    // Egress: don't wait, just walk. Get to destination as early as possible
                    else if (ss.deparr_mode_ == MODE_EGRESS) {
                        path[curr_index].second.deparr_time_ = path[prev_index].second.arrdep_time_;
                        path[curr_index].second.arrdep_time_ = path[curr_index].second.deparr_time_ + path[curr_index].second.link_time_;
                    }
                }
                // INBOUND: We are choosing links in REVERSE chronological order
                else
                {
                    // Leave origin as late as possible
                    if (path[curr_index].second.deparr_mode_ == MODE_ACCESS) {
                        path[curr_index].second.deparr_time_ = path[prev_index].second.arrdep_time_;
                        path[curr_index].second.arrdep_time_ = path[curr_index].second.deparr_time_ - path[curr_index].second.link_time_;
                        // no wait time for the trip
                        path[prev_index].second.link_time_   = path[prev_index].second.deparr_time_ - path[prev_index].second.arrdep_time_;
                    }
                    // *Trip* - fix transfer and next trip if applicable
                    else if (isTrip(path[curr_index].second.deparr_mode_)) {
                        // If we just picked this trip and the previous (next in time) is transfer then we know the wait now
                        // and we can update the transfer and the trip with the real wait
                        if (path[prev_index].second.deparr_mode_ == MODE_TRANSFER) {
                            // move transfer time so we do it right after arriving
                            path[prev_index].second.arrdep_time_ = path[curr_index].second.deparr_time_; // depart right away
                            path[prev_index].second.deparr_time_ = path[curr_index].second.deparr_time_ + path[prev_index].second.link_time_; // arrive after walk
                            // give the wait time to the previous trip
                            path[prev_index-1].second.link_time_ = path[prev_index-1].second.deparr_time_ - path[prev_index].second.deparr_time_;
                        }
                        // If the previous (next in time) is another trip (so zero-walk transfer) give it wait time
                        else if (isTrip(path[prev_index].second.deparr_mode_)) {
                            path[prev_index].second.link_time_ = path[prev_index].second.deparr_time_ - path[curr_index].second.deparr_time_;
                        }
                    }
                    // Egress: don't wait, just walk. Get to destination as early as possible
                    if (path[prev_index].second.deparr_mode_ == MODE_EGRESS) {
                        path[prev_index].second.arrdep_time_ = ss.deparr_time_;
                        path[prev_index].second.deparr_time_ = path[prev_index].second.arrdep_time_ + path[prev_index].second.link_time_;
                    }
                }
            }
            calculatePathCost(path_spec, trace_file, path, path_info);
        }
        if (path_spec.trace_)
        {
            trace_file << "Final path" << std::endl;
            printPath(trace_file, path_spec, path);
        }
    }

    /**
     * Returns the departure time for the transit vehicle from the given stop/seq for the given trip.
     * Returns -1 on failure.
     */
    double PathFinder::getScheduledDeparture(int trip_id, int stop_id, int sequence) const
    {
        std::map<int, std::vector<TripStopTime> >::const_iterator tsti = trip_stop_times_.find(trip_id);
        if (tsti == trip_stop_times_.end()) { return -1; }

        for (size_t stt_index = 0; stt_index < tsti->second.size(); ++stt_index)
        {
            if (tsti->second[stt_index].stop_id_ != stop_id) { continue; }
            // trip id matches and stop id matches -- does sequence match or is it unspecified?
            if ((sequence < 0) || (sequence == tsti->second[stt_index].seq_)) {
                return tsti->second[stt_index].depart_time_;
            }
        }
        return -1;
    }

    /**
     * If outbound, then we're searching backwards, so this returns trips that arrive at the stop in time to depart at timepoint (timepoint-TIME_WINDOW_, timepoint]
     * If inbound,  then we're searching forwards,  so this returns trips that depart at the stop time after timepoint           [timepoint, timepoint+TIME_WINDOW_)
     */
    void PathFinder::getTripsWithinTime(int stop_id, bool outbound, double timepoint, std::vector<TripStopTime>& return_trips) const
    {
        // are there any trips for this stop?
        std::map<int, std::vector<TripStopTime> >::const_iterator mapiter = stop_trip_times_.find(stop_id);
        if (mapiter == stop_trip_times_.end()) {
            return;
        }
        for (std::vector<TripStopTime>::const_iterator it  = mapiter->second.begin();
                                                       it != mapiter->second.end();   ++it) {
            if (outbound && (it->arrive_time_ <= timepoint) && (it->arrive_time_ > timepoint-TIME_WINDOW_)) {
                return_trips.push_back(*it);
            } else if (!outbound && (it->depart_time_ >= timepoint) && (it->depart_time_ < timepoint+TIME_WINDOW_)) {
                return_trips.push_back(*it);
            }
        }
    }

    double PathFinder::calculateNonwalkLabel(const std::vector<StopState>& current_stop_state) const
    {
        double nonwalk_label = 0.0;
        for (std::vector<StopState>::const_iterator it = current_stop_state.begin();
             it != current_stop_state.end(); ++it)
        {
            if ((it->deparr_mode_ != MODE_EGRESS  ) &&
                (it->deparr_mode_ != MODE_TRANSFER) &&
                (it->deparr_mode_ != MODE_ACCESS  ))
            {
                nonwalk_label += exp(-1.0*STOCH_DISPERSION_*it->cost_);
            }
        }

        if (nonwalk_label == 0.0) {
            return PathFinder::MAX_COST;
        }
        return -1.0/STOCH_DISPERSION_*log(nonwalk_label);
    }

    void PathFinder::printPath(std::ostream& ostr, const PathSpecification& path_spec, const Path& path) const
    {
        printStopStateHeader(ostr, path_spec);
        ostr << std::endl;
        for (int index = 0; index < path.size(); ++index)
        {
            printStopState(ostr, path[index].first, path[index].second, path_spec);
            ostr << std::endl;
        }
    }

    void PathFinder::printPathCompat(std::ostream& ostr, const PathSpecification& path_spec, const Path& path) const
    {
        if (path.size() == 0)
        {
            ostr << "no_path";
            return;
        }
        // board stops, trips, alight stops
        std::string board_stops, trips, alight_stops;
        int start_ind = path_spec.outbound_ ? 0 : path.size()-1;
        int end_ind   = path_spec.outbound_ ? path.size() : -1;
        int inc       = path_spec.outbound_ ? 1 : -1;
        for (int index = start_ind; index != end_ind; index += inc)
        {
            int stop_id = path[index].first;
            // only want trips
            if (path[index].second.deparr_mode_ == MODE_ACCESS  ) { continue; }
            if (path[index].second.deparr_mode_ == MODE_EGRESS  ) { continue; }
            if (path[index].second.deparr_mode_ == MODE_TRANSFER) { continue; }
            if ( board_stops.length() > 0) {  board_stops += ","; }
            if (       trips.length() > 0) {        trips += ","; }
            if (alight_stops.length() > 0) { alight_stops += ","; }
            board_stops  += (path_spec.outbound_ ? stop_num_to_str_.find(stop_id)->second : stop_num_to_str_.find(path[index].second.stop_succpred_)->second);
            trips        += trip_num_to_str_.find(path[index].second.trip_id_)->second;
            alight_stops += (path_spec.outbound_ ? stop_num_to_str_.find(path[index].second.stop_succpred_)->second : stop_num_to_str_.find(stop_id)->second);
        }
        ostr << " " << board_stops << " " << trips << " " << alight_stops;
    }


    void PathFinder::printStopStateHeader(std::ostream& ostr, const PathSpecification& path_spec) const
    {
        ostr << std::setw( 8) << std::setfill(' ') << std::right << "stop" << ": ";
        ostr << std::setw(10) << (path_spec.outbound_ ? "dep_time" : "arr_time");
        ostr << std::setw(12) << (path_spec.outbound_ ? "dep_mode" : "arr_mode");
        ostr << std::setw(22) << "trip_id";
        ostr << std::setw(12) << (path_spec.outbound_ ? "successor" : "predecessor");
        ostr << std::setw( 5) << "seq";
        ostr << std::setw( 5) << (path_spec.outbound_ ? "suc" : "pred");
        ostr << std::setw(12) << "linktime";
        ostr << std::setw(14) << "linkcost";
        ostr << std::setw(13) << "cost";
        ostr << std::setw( 9) << "iter";
        ostr << std::setw(10) << (path_spec.outbound_ ? "arr_time" : "dep_time");
    }

    void PathFinder::printStopState(std::ostream& ostr, int stop_id, const StopState& ss, const PathSpecification& path_spec) const
    {
        ostr << std::setw( 8) << std::setfill(' ') << std::right << stop_num_to_str_.find(stop_id)->second << ":   ";
        printTime(ostr, ss.deparr_time_);
        ostr << "  ";
        printMode(ostr, ss.deparr_mode_, ss.trip_id_);
        ostr << "  ";
        if (ss.deparr_mode_ == MODE_TRANSIT) {
            ostr << std::setw(20) << std::setfill(' ') << trip_num_to_str_.find(ss.trip_id_)->second;
        } else if (ss.deparr_mode_ == MODE_ACCESS || ss.deparr_mode_ == MODE_EGRESS) {
            ostr << std::setw(20) << std::setfill(' ') << mode_num_to_str_.find(ss.trip_id_)->second;
        } else {
            ostr << std::setw(20) << std::setfill(' ') << ss.trip_id_;
        }
        ostr << "  ";
        ostr << std::setw(10) << std::setfill(' ') << stop_num_to_str_.find(ss.stop_succpred_)->second;
        ostr << "  ";
        ostr << std::setw(3) << std::setfill(' ') << ss.seq_;
        ostr << "  ";
        ostr << std::setw(3) << std::setfill(' ') << ss.seq_succpred_;
        ostr << "  ";
        printTimeDuration(ostr, ss.link_time_);
        ostr << "  ";
        if (path_spec.hyperpath_) {
            ostr << std::setw(12) << std::setprecision(4) << std::fixed << std::setfill(' ') << ss.link_cost_;
            ostr << std::setw(13) << std::setprecision(4) << std::fixed << std::setfill(' ') << ss.cost_;
        } else {
            // cost is a time duration
            ostr << "  ";
            printTimeDuration(ostr, ss.link_cost_);
            ostr << "  ";
            printTimeDuration(ostr, ss.cost_);
        }
        ostr << "  ";
        ostr << std::setw(7) << std::setfill(' ') << ss.iteration_;
        ostr << "  ";
        printTime(ostr, ss.arrdep_time_);
    }

    /*
     * Assuming that timedur is duration in minutes, prints a formatted version.
     */
    void PathFinder::printTimeDuration(std::ostream& ostr, const double& timedur) const
    {
        int hours = static_cast<int>(timedur/60.0);
        double minutes = timedur - 60.0*hours;
        double minpart, secpart;
        secpart = modf(minutes, &minpart);
        secpart = secpart*60.0;
        // double intpart, fracpart;
        // fracpart = modf(secpart, &intpart);
        ostr << std::right;
        ostr << std::setw( 2) << std::setfill(' ') << std::right << hours << ":"; // hours
        ostr << std::setw( 2) << std::setfill('0') << static_cast<int>(minpart)      << ":"; // minutes
        int width = 3;
        if (secpart < 10) { ostr << "0"; width = 2; }
        ostr << std::left << std::setw(width) << std::setprecision( 1) << std::fixed << std::setfill(' ') << secpart << std::right; // seconds
    }

    /*
     * Assuming that timemin is a time in minutes after midnight, prints a formatted version.
     */
    void PathFinder::printTime(std::ostream& ostr, const double& timemin) const
    {
        double minpart, secpart;
        int    hour = static_cast<int>(timemin/60.0);

        secpart = modf(timemin, &minpart); // split into minutes and seconds
        minpart = minpart - hour*60.0;
        secpart = secpart*60.0;
        ostr << std::right;
        ostr << std::setw( 2) << std::setfill('0') << hour                       << ":"; // hour
        ostr << std::setw( 2) << std::setfill('0') << static_cast<int>(minpart)  << ":"; // minutes
        ostr << std::setw( 2) << std::setfill('0') << static_cast<int>(secpart);
    }

    void PathFinder::printMode(std::ostream& ostr, const int& mode, const int& trip_id) const
    {
        if (mode == MODE_ACCESS) {
            ostr << std::setw(10) << std::setfill(' ') << "Access";
        } else if (mode == MODE_EGRESS) {
            ostr << std::setw(10) << std::setfill(' ') << "Egress";
        } else if (mode == MODE_TRANSFER) {
            ostr << std::setw(10) << std::setfill(' ') << "Transfer";
        } else if (mode == MODE_TRANSIT) {
            // show the supply mode
            int supply_mode_num = trip_info_.find(trip_id)->second.supply_mode_num_;
            ostr << std::setw(10) << std::setfill(' ') << mode_num_to_str_.find(supply_mode_num)->second;
        } else {
            // trip
            ostr << std::setw(10) << std::setfill(' ') << "???";
        }
    }

    bool PathFinder::isTrip(const int& mode) const
    {
        return (mode == MODE_TRANSIT);
    }

}
