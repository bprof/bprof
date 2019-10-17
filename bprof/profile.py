class BaseFunction:
    def __init__(self, name, n_calls, internal_ns):
        self._name = name
        self._n_calls = n_calls
        self._internal_ns = internal_ns

    @property
    def name(self):
        return self._name

    @property
    def n_calls(self):
        return self._n_calls

    @property
    def internal_ns(self):
        return self._internal_ns


class Lines:
    def __init__(self, line_str, n_calls, internal, external):
        self._line_str = line_str
        self._n_calls = n_calls
        self._internal = internal
        self._external = external

    @property
    def text(self):
        return self._line_str

    @property
    def n_calls(self):
        return self._n_calls

    @property
    def internal(self):
        return self._internal

    @property
    def external(self):
        return self._external

    @property
    def total(self):
        return self.internal + self.external


class Function(BaseFunction):
    def __init__(self, name, lines, n_calls, internal_ns):
        self._name = name
        self._lines = lines
        self._n_calls = n_calls
        self._internal_ns = internal_ns

    @property
    def lines(self):
        return self._lines

    @property
    def name(self):
        return self._name

    @property
    def n_calls(self):
        return self._n_calls

    @property
    def internal_ns(self):
        return self._internal_ns

    @property
    def total(self):
        tot = 0
        for line in self.lines:
            tot += line.total
        return tot + self.internal_ns


class Profile:
    @staticmethod
    def from_data(data):
        profile = Profile()
        profile._functions = []
        
        for key, fdata in data['functions'].items():
            lines = []
            for line in fdata['lines']:
                line = Lines(line['line_str'], line['n_calls'], 
                             line['internal_ns'], line['external_ns'])
                lines.append(line)

            func = Function(lines=lines, name=fdata['name'], 
                            n_calls=fdata['n_calls'],
                            internal_ns=fdata['internal_ns'])
            profile._functions.append(func)

        return profile

    @property
    def functions(self):
        return self._functions
