
#include <array>
#include <vector>


class PerturbMultiplier
{
public :

	PerturbMultiplier( std::array<long, 3>		values )
		: m_multipliers( values )
	{}


private :

	union
	{
		std::array<long,3>			m_multipliers;

		struct
		{
			long		m_x;
			long		m_y;
			long		m_z;
		};
	};

};

typedef std::vector<PerturbMultiplier>		PerturbMultipliers;

27, 64, 


std::array<PerturbMultiplier,64>		firstRow
( { PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,1 } ),
	PerturbMultiplier( { 0,0,2 } ),
	PerturbMultiplier( { 0,1,0 } ),
	PerturbMultiplier( { 0,1,1 } ),
	PerturbMultiplier( { 0,1,2 } ),
	PerturbMultiplier( { 0,2,0 } ),
	PerturbMultiplier( { 0,2,1 } ),
	PerturbMultiplier( { 0,2,2 } ),
	PerturbMultiplier( { 1,0,0 } ),
	PerturbMultiplier( { 1,0,1 } ),
	PerturbMultiplier( { 1,0,2 } ),
	PerturbMultiplier( { 1,1,0 } ),
	PerturbMultiplier( { 1,1,1 } ),
	PerturbMultiplier( { 1,1,2 } ),
	PerturbMultiplier( { 1,2,0 } ),
	PerturbMultiplier( { 1,2,1 } ),
	PerturbMultiplier( { 1,2,2 } ),
	PerturbMultiplier( { 2,0,0 } ),
	PerturbMultiplier( { 2,0,1 } ),
	PerturbMultiplier( { 2,0,2 } ),
	PerturbMultiplier( { 2,1,0 } ),
	PerturbMultiplier( { 2,1,1 } ),
	PerturbMultiplier( { 2,1,2 } ),
	PerturbMultiplier( { 2,2,0 } ),
	PerturbMultiplier( { 2,2,1 } ),
	PerturbMultiplier( { 2,2,2 } ),
	PerturbMultiplier( { 0,0,3 } ),
	PerturbMultiplier( { 0,1,3 } ),
	PerturbMultiplier( { 0,2,3 } ),
	PerturbMultiplier( { 0,3,0 } ),
	PerturbMultiplier( { 0,3,1 } ),
	PerturbMultiplier( { 0,3,2 } ),
	PerturbMultiplier( { 0,3,3 } ),
	PerturbMultiplier( { 1,0,3 } ),
	PerturbMultiplier( { 1,1,3 } ),
	PerturbMultiplier( { 1,2,3 } ),
	PerturbMultiplier( { 1,3,0 } ),
	PerturbMultiplier( { 1,3,1 } ),
	PerturbMultiplier( { 1,3,2 } ),
	PerturbMultiplier( { 1,3,3 } ),
	PerturbMultiplier( { 2,0,3 } ),
	PerturbMultiplier( { 2,1,3 } ),
	PerturbMultiplier( { 2,2,3 } ),
	PerturbMultiplier( { 2,3,0 } ),
	PerturbMultiplier( { 2,3,1 } ),
	PerturbMultiplier( { 2,3,2 } ),
	PerturbMultiplier( { 2,3,3 } ),
	PerturbMultiplier( { 3,0,0 } ),
	PerturbMultiplier( { 3,0,1 } ),
	PerturbMultiplier( { 3,0,2 } ),
	PerturbMultiplier( { 3,1,0 } ),
	PerturbMultiplier( { 3,1,1 } ),
	PerturbMultiplier( { 3,1,2 } ),
	PerturbMultiplier( { 3,2,0 } ),
	PerturbMultiplier( { 3,2,1 } ),
	PerturbMultiplier( { 3,2,2 } ),
	PerturbMultiplier( { 3,0,3 } ),
	PerturbMultiplier( { 3,1,3 } ),
	PerturbMultiplier( { 3,2,3 } ),
	PerturbMultiplier( { 3,3,0 } ),
	PerturbMultiplier( { 3,3,1 } ),
	PerturbMultiplier( { 3,3,2 } ),
	PerturbMultiplier( { 3,3,3 } ),
	PerturbMultiplier( { 0,0,4 } ),
	PerturbMultiplier( { 0,1,4 } ),
	PerturbMultiplier( { 0,2,4 } ),
	PerturbMultiplier( { 0,3,4 } ),
	PerturbMultiplier( { 0,,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,0 } ),

std::array<PerturbMultiplier, 64>		secondRow
( { PerturbMultiplier( { 0,0,0 } ),
	PerturbMultiplier( { 0,0,1 } ),
	PerturbMultiplier( { 0,0,2 } ),
	PerturbMultiplier( { 0,0,3 } ),
	PerturbMultiplier( { 0,1,0 } ),
	PerturbMultiplier( { 0,1,1 } ),
	PerturbMultiplier( { 0,1,2 } ),
	PerturbMultiplier( { 0,1,3 } ),
	PerturbMultiplier( { 0,2,0 } ),
	PerturbMultiplier( { 0,2,1 } ),
	PerturbMultiplier( { 0,2,2 } ),
	PerturbMultiplier( { 0,2,3 } ),
	PerturbMultiplier( { 0,3,0 } ),
	PerturbMultiplier( { 0,3,1 } ),
	PerturbMultiplier( { 0,3,2 } ),
	PerturbMultiplier( { 0,3,3 } ),
	PerturbMultiplier( { 1,0,0 } ),
	PerturbMultiplier( { 1,0,1 } ),
	PerturbMultiplier( { 1,0,2 } ),
	PerturbMultiplier( { 1,0,3 } ),
	PerturbMultiplier( { 1,1,0 } ),
	PerturbMultiplier( { 1,1,1 } ),
	PerturbMultiplier( { 1,1,2 } ),
	PerturbMultiplier( { 1,1,3 } ),
	PerturbMultiplier( { 1,2,0 } ),
	PerturbMultiplier( { 1,2,1 } ),
	PerturbMultiplier( { 1,2,2 } ),
	PerturbMultiplier( { 1,2,3 } ),
	PerturbMultiplier( { 1,3,0 } ),
	PerturbMultiplier( { 1,3,1 } ),
	PerturbMultiplier( { 1,3,2 } ),
	PerturbMultiplier( { 1,3,3 } ),
	PerturbMultiplier( { 2,0,0 } ),
	PerturbMultiplier( { 2,0,1 } ),
	PerturbMultiplier( { 2,0,2 } ),
	PerturbMultiplier( { 2,0,3 } ),
	PerturbMultiplier( { 2,1,0 } ),
	PerturbMultiplier( { 2,1,1 } ),
	PerturbMultiplier( { 2,1,2 } ),
	PerturbMultiplier( { 2,1,3 } ),
	PerturbMultiplier( { 2,2,0 } ),
	PerturbMultiplier( { 2,2,1 } ),
	PerturbMultiplier( { 2,2,2 } ),
	PerturbMultiplier( { 2,2,3 } ),
	PerturbMultiplier( { 2,3,0 } ),
	PerturbMultiplier( { 2,3,1 } ),
	PerturbMultiplier( { 2,3,2 } ),
	PerturbMultiplier( { 2,3,3 } ),
	PerturbMultiplier( { 3,0,0 } ),
	PerturbMultiplier( { 3,0,1 } ),
	PerturbMultiplier( { 3,0,2 } ),
	PerturbMultiplier( { 3,0,3 } ),
	PerturbMultiplier( { 3,1,0 } ),
	PerturbMultiplier( { 3,1,1 } ),
	PerturbMultiplier( { 3,1,2 } ),
	PerturbMultiplier( { 3,1,3 } ),
	PerturbMultiplier( { 3,2,0 } ),
	PerturbMultiplier( { 3,2,1 } ),
	PerturbMultiplier( { 3,2,2 } ),
	PerturbMultiplier( { 3,2,3 } ),
	PerturbMultiplier( { 3,3,0 } ),
	PerturbMultiplier( { 3,3,1 } ),
	PerturbMultiplier( { 3,3,2 } ),
	PerturbMultiplier( { 3,3,3 } ) } );


