
function BranchTraceReaderV5(buffer) {
	this.buffer = buffer

	this.version = null
	
	this.ntables = null
	this.nrows = null
	this.nbanks = null

	this.trainingEventsSeen = 0

	this.asyncInit = function(onReady) {
		const self = this
		this.awaitReadyToRead(function() {
			self.version = self.buffer.readInt8()
			if (self.version != 4) 
				alert('opened file with version ' + self.version + ' instead of expected 4')

			self.ntables = self.buffer.readUInt32()
			self.nrows = self.buffer.readUInt32()
			self.nbanks = self.buffer.readUInt32()		

			onReady()
		})
	}

	const MAX_RECORD_SIZE = 60 // doesn't have to be exact, just >= actual max record size
	this.readyToRead = function() {
		return this.buffer.remaining() >= MAX_RECORD_SIZE || (this.buffer.fullyBuffered() && this.buffer.remaining() > 0)
	}

	this.eof = function() {
		return this.buffer.fullyBuffered() && this.buffer.remaining() <= 0 // Catches incomplete files somehow
	}

	// Async
	this.awaitReadyToRead = function(onReady) {
		buffer.makeReady(MAX_RECORD_SIZE, onReady)
	}

	// Returns: ['T', ...] for training events
	// Returns: ['W', ...] for per-weight training information following a training event.
	//                     All of these following one training event correspond to that training event
	this.nextRecord = function() {
		if (this.eof())
			return null;

		let type = this.buffer.readInt8()
		if (type == 'T'.charCodeAt(0)) { // Training record
			if (this.buffer.remaining() < 42)
				return null; // TEMP: not enough data: probably killed simulation without flushing data or truncated the file

			let cycle = this.buffer.readInt64AsDouble()
			let pcLo = this.buffer.readUInt32()
			let pcHi = this.buffer.readUInt32()
			let correct = this.buffer.readInt8()
			let taken = this.buffer.readInt8()
			let tgtLo = this.buffer.readUInt32()
			let tgtHi = this.buffer.readUInt32()
			let yout = this.buffer.readInt32()
			let bias_at_lookup = this.buffer.readInt16()
			let theta_at_training = this.buffer.readInt32()
			let bias_at_training = this.buffer.readInt16()
			let shpq_weights_found = this.buffer.readInt8()
			let dynamic_state = this.buffer.readInt8()
			let indirect = this.buffer.readInt8()
			let uncond = this.buffer.readInt8()

			//console.log(
			//	'T cycle=' + cycle +
			//	' pcLo=' + pcLo +
			//	' pcHi=' + pcHi +
			//	' correct=' + correct +
			//	' taken=' + taken +
			//	' tgtLo=' + tgtLo +
			//	' tgtHi=' + tgtHi +
			//	' yout=' + yout +
			//	' bias_at_lookup=' + bias_at_lookup +
			//	' theta_at_training=' + theta_at_training +
			//	' bias_at_training=' + bias_at_training +
			//	' shpq_weights_found=' + shpq_weights_found +
			//	' dynamic_state=' + dynamic_state +
			//	' indirect=' + indirect +
			//	' uncond=' + uncond
			//	)

			this.trainingEventsSeen++

			return ['T', cycle, pcLo, pcHi,
				    correct, taken,
				    yout, theta_at_training, bias_at_training,
				    shpq_weights_found, dynamic_state, indirect, uncond, bias_at_lookup, tgtLo, tgtHi]

		} else if (type == 'W'.charCodeAt(0)) { // Write record
			if (this.trainingEventsSeen == 0)
				console.error('No training events seen before first write event. Something might be wrong with the file')

			if (this.buffer.remaining() < 20)
				return null; // TEMP: not enough data: probably killed simulation without flushing data or truncated the file

			let table = this.buffer.readInt32()
			let row = this.buffer.readInt32()
			let bank = this.buffer.readInt32()
			let lookup_weight = this.buffer.readInt16()
			let new_weight = this.buffer.readInt16()
			let d_weight = this.buffer.readInt16()
			let d_unique = this.buffer.readInt8()
			let thrash_1 = this.buffer.readInt8()

			//console.log(
			//	'W table=' + table +
			//	' row=' + row +
			//	' bank=' + bank +
			//	' lookup_weight=' + lookup_weight +
			//	' new_weight=' + new_weight +
			//	' d_weight=' + d_weight +
			//	' d_unique=' + d_unique +
			//	' thrash_1=' + thrash_1
			//	)

			let flatTableIdx = table * this.nbanks + bank
			return ['W',
					flatTableIdx, row,
					lookup_weight, new_weight, d_weight,
				    d_unique, thrash_1] 
				    //, contrib_mpred, contra_mpred, weight_correct, weight_incorrect]
		} else {
			console.error('Unknown record type' + type)
		}
	}
}