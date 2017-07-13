/***********************************************************************************
Copyright (c) 2017, Michael Neunert, Markus Giftthaler, Markus Stäuble, Diego Pardo,
Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of ETH ZURICH nor the names of its contributors may be used
      to endorse or promote products derived from this software without specific
      prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL ETH ZURICH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************/

template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::createLQProblem()
{
	this->sequentialLQProblem();
}

template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::backwardPass()
{
	// step 3
	// initialize cost to go (described in step 3)
	this->initializeCostToGo();

	ct::core::Timer timer;

#ifdef HPIPM
	ct::core::StateVectorArray<STATE_DIM> b(this->K_);
	for (int k=0; k<this->K_; k++)
	{
//		this->getNonlinearSystemsInstances()[this->settings_.nThreads]->computeControlledDynamics(
//				ct::core::StateVector<STATE_DIM>::Zero(),
//				0.0,
//				ct::core::ControlVector<CONTROL_DIM>::Zero(),
//				der[k]
//		);
		b[k] = this->x_[k+1] - this->A_[k] * this->x_[k] - this->B_[k] * this->u_ff_[k] + this->d_[k];
	}

	this->HPIPMInterface_.setProblem(
			this->x_,
			this->u_ff_,
			this->A_,
			this->B_,
			b,
			this->P_,
			this->qv_,
			this->Q_,
			this->rv_,
			this->R_
	);



	timer.start();

	this->HPIPMInterface_.solve();

	timer.stop();
	std::cout << "time HPIPM: "<<timer.getElapsedTime()<<std::endl;


	//this->HPIPMInterface_.printSolution();

#endif HPIPM



	this->du_norm_ = 0;
	this->dx_norm_ = 0;

	timer.start();

	for (int k=this->K_-1; k>=0; k--)
	{
		// design controller
		this->designController(k);

		// compute cost to go
		this->computeCostToGo(k);
	}


	for (int k=0; k<this->K_; k++)
	{
		// design controller
		this->designStateUpdate(k);
		this->dx_norm_ += this->lx_[k+1].norm();
	}

	updateControlAndState();

	timer.stop();

	std::cout << "time CT: "<<timer.getElapsedTime()<<std::endl;
}


template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::computeLinearizedDynamicsAroundTrajectory()
{
	for (size_t k=0; k<this->K_; k++)
	{
		// step 2
		// computes linearized dynamics
		this->computeLinearizedDynamics(this->settings_.nThreads, k);
	}
}


template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::computeQuadraticCostsAroundTrajectory()
{
	for (size_t k=0; k<this->K_; k++)
	{
		// compute quadratic cost
		this->computeQuadraticCosts(this->settings_.nThreads, k);
	}
}

template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::updateControlAndState()
{
	for (size_t k=0; k<this->K_; k++)
	{
		// compute quadratic cost
		this->updateSingleControlAndState(this->settings_.nThreads, k);
	}

	this->x_[this->K_] += this->lx_[this->K_];
}

template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::initializeShots()
{
	for (size_t k=0; k<this->K_; k++) {
		this->initializeSingleShot(this->settings_.nThreads, k);
	}
}

template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::updateShots()
{
	for (size_t k=0; k<this->K_; k++) {
		this->updateSingleShot(this->settings_.nThreads, k);
		//this->initializeSingleShot(this->settings_.nThreads, k);
	}
}

template <size_t STATE_DIM, size_t CONTROL_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, SCALAR>::computeDefects()
{
	this->d_norm_ = 0.0;

	for (size_t k=0; k<this->K_+1; k++) {
		this->computeSingleDefect(this->settings_.nThreads, k);
		this->d_norm_ += this->d_[k].norm();
	}
}

