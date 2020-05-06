

function BranchClass(dynamic_state, indirect, uncond) {
	this.dynamic_state = dynamic_state
	this.indirect = indirect
	this.uncond = uncond

	this.getString = function() {
        let bclassStr = ''
        if (this.indirect) {
            bclassStr += 'i'
        } else {
            bclassStr += 'd'
        }
        if (this.uncond) {
            bclassStr += 'u'
        } else {
            bclassStr += 'c'
        }
        if (this.dynamic_state == 0) {
            bclassStr += ' '
        } else if (this.dynamic_state == 1) {
            bclassStr += ' AT'
        } else if (this.dynamic_state == 2) {
            bclassStr += ' ANT'
        } else if (this.dynamic_state == 3) {
            bclassStr += ' TNT' // Probably always TNT since this just just SHP-visible branches
        } else {
            bclassStr += ' ??'
        }

        return bclassStr
	}
}

function toPaddedHex8(num) {
	//let s = Math.abs(num).toString(16)
	let s = num.toString(16)
	if (s.length < 8) {
		s = s.padStart(8 - s.length, '0')
	}
	return s
}

function renderBranchAddress(pcLo, pcHi) {
	return '0x' + toPaddedHex8(pcHi) + toPaddedHex8(pcLo)
}

function BranchProfile(pcStr, instances=1, count1=1, count2=0, weightCorrect=0, bclass=null, branchCorrect=0) {
	this.instances = instances
	this.count1 = count1
	this.count2 = count2
	this.weightCorrect = weightCorrect
	this.pcStr = pcStr
	this.bclass = null // BranchClass
	this.branchCorrect = branchCorrect

	this.add = function(branchProfile) {
		this.instances += branchProfile.instances
		this.count1 += branchProfile.count1
		this.count2 += branchProfile.count2
		this.weightCorrect += branchProfile.weightCorrect
		this.bclass = branchProfile.bclass // Assume latest value
		this.branchCorrect += branchProfile.branchCorrect
	}

	this.addBranches = function(instances, c1, c2, weightCorrect, bclass, branchCorrect) {
		this.instances += instances
		this.count1 += c1
		this.count2 += c2
		this.weightCorrect += weightCorrect
		this.bclass = bclass // Assume latest value
		this.branchCorrect += branchCorrect
	}

	this.mispredictCount = function() {
		return this.instances - this.branchCorrect
	}
}

function BranchProfileManager () {
	this.branches = new Map()

	// Add new instance/counter values and set new branch class
	this.addBranches = function(pcStr, instances, c1, c2, weightCorrect, bclass, branchCorrect) {
		if (!this.branches.has(pcStr)) {
			this.branches.set(pcStr, new BranchProfile(pcStr, instances, c1, c2, weightCorrect, bclass, branchCorrect))
		} else {
			this.branches.get(pcStr).addBranches(instances, c1, c2, weightCorrect, bclass, branchCorrect)
		}
	}

	this.mergeBranchProfile = function(branchProfile) {
		if (!this.branches.has(branchProfile.pcStr)) {
			this.branches.set(branchProfile.pcStr, branchProfile)
		} else {
			this.branches.get(branchProfile.pcStr).add(branchProfile)
		}
	}

	this.size = function() {
		return this.branches.size()
	}

	this.getDescendingList = function() {
		let l = Array.from(this.branches.values())
		function reverseCmp(a,b) {
			if (a.instances > b.instances)
				return -1 // a > b but ordering descending
			if (a.instances < b.instances)
				return 1 // a < b but ordering descending
			return 0
		}
		return l.sort(reverseCmp)
	}

	this.getDescendingListByMispredictCount = function() {
		let l = Array.from(this.branches.values())
		function reverseCmp(a,b) {
			if (a.mispredictCount() > b.mispredictCount())
				return -1 // a > b but ordering descending
			if (a.mispredictCount() < b.mispredictCount())
				return 1 // a < b but ordering descending
			return 0
		}
		return l.sort(reverseCmp)
	}

}
