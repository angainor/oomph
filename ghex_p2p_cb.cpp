#include <iostream>
#include <vector>

#include <ghex/common/timer.hpp>


#ifdef USE_MPI

/* MPI backend */
#include <ghex/transport_layer/callback_communicator.hpp>
#include <ghex/transport_layer/mpi/communicator.hpp>
using CommType = gridtools::ghex::tl::communicator<gridtools::ghex::tl::mpi_tag>;
#else

/* UCX backend */
#ifdef USE_UCX_NBR
#include <ghex/transport_layer/callback_communicator.hpp>
#else
#include <ghex/transport_layer/ucx/callback_communicator.hpp>
#endif
#include <ghex/transport_layer/ucx/communicator.hpp>
using CommType = gridtools::ghex::tl::communicator<gridtools::ghex::tl::ucx_tag>;
#endif /* USE_MPI */

using MsgType = gridtools::ghex::tl::shared_message_buffer<>;

/* comm requests currently in-flight */
int ongoing_comm = 0;

void send_callback(MsgType mesg, int rank, int tag)
{
    // std::cout << "send callback called " << rank << " thread " << omp_get_thread_num() << " tag " << tag << "\n";
    ongoing_comm--;
}

void recv_callback(MsgType mesg, int rank, int tag)
{
    // std::cout << "recv callback called " << rank << " thread " << omp_get_thread_num() << " tag " << tag << "\n";
    ongoing_comm--;
}

int main(int argc, char *argv[])
{
    int rank, size, threads, peer_rank;
    int niter, buff_size;
    int inflight;

#ifdef USE_MPI
    int mode;
#ifdef THREAD_MODE_MULTIPLE
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &mode);
    if(mode != MPI_THREAD_MULTIPLE){
	std::cerr << "MPI_THREAD_MULTIPLE not supported by MPI, aborting\n";
	std::terminate();
    }
#else
    MPI_Init_thread(NULL, NULL, MPI_THREAD_SINGLE, &mode);
#endif
#endif

    gridtools::ghex::tl::callback_communicator<CommType> comm;
    
    niter = atoi(argv[1]);
    buff_size = atoi(argv[2]);
    inflight = atoi(argv[3]);   
    
    rank = comm.rank();
    size = comm.size();
    peer_rank = (rank+1)%2;

    if(rank==0)	std::cout << "\n\nrunning test " << __FILE__ << " with communicator " << typeid(comm).name() << "\n\n";

    {
	gridtools::ghex::timer timer;
	long bytes = 0;
	std::vector<MsgType> msgs;
	
	for(int j=0; j<inflight; j++){
	    msgs.emplace_back(buff_size);
	}
	
	if(rank == 1) {
	    timer.tic();
	    bytes = (double)niter*size*buff_size/2;
	}

	/* send / recv niter messages, work in inflight requests at a time */
	int i = 0;
	while(i<niter){

	    /* submit inflight requests */
	    for(int j=0; j<inflight; j++){
		if(rank==0 && (i)%(niter/10)==0) {
		    std::cout << i << " iters\n";
		}
		i++;
		ongoing_comm++;
		if(rank==0)
		    comm.send(msgs[j], peer_rank, j, send_callback);
		else
		    comm.recv(msgs[j], peer_rank, j, recv_callback);
		if(i==niter) break;
	    }
		
	    /* complete all inflight requests before moving on */
	    while(ongoing_comm){
		comm.progress();
	    }
	}

	comm.flush();
	comm.barrier();
	
	if(rank == 1) timer.vtoc(bytes);
    }

#ifdef USE_MPI
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
#endif
}
